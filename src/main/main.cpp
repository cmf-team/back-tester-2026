#include "common/Events.hpp"
#include "common/Pipeline.hpp"
#include "data_layer/JsonMarketDataFolderLoader.hpp"
#include "transport/Subscriber.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

namespace {

void printVerificationLine(std::ostream &os,
                             const domain::events::MarketDataEvent &event) {
  os << "ts_event_ns=" << event.hd.ts_event << " ts_recv=" << event.ts_recv
     << " order_id=" << event.order_id << " side=" << event.side << " price=";
  if (event.price == domain::events::UNDEF_PRICE) {
    os << "null";
  } else {
    os << event.price;
  }
  os << " size=" << event.size << " action=" << event.action;
}

void processMarketDataEvent(const domain::events::MarketDataEvent &order) {
  printVerificationLine(std::cout, order);
  std::cout << '\n';
}

void dumpFirstLastTen(
    std::queue<domain::events::MarketDataEvent> &first_ten,
    std::queue<domain::events::MarketDataEvent> &last_ten) {
  std::cout << "First ten print:\n";
  while (!first_ten.empty()) {
    processMarketDataEvent(first_ten.front());
    first_ten.pop();
  }
  std::cout << "Last ten print:\n";
  while (!last_ten.empty()) {
    processMarketDataEvent(last_ten.front());
    last_ten.pop();
  }
}

struct Stats {
  std::string name;
  std::size_t processed_events = 0;
  double elapsed_seconds = 0.0;
  bool have_timestamps = false;
  std::uint64_t first_ts_event = 0;
  std::uint64_t last_ts_event = 0;
};

template <typename QueueT>
Stats runPipelineBenchmark(const std::string &name,
                             const std::vector<std::string> &json_files,
                             bool hard_mode, bool emit_event_logs) {
  const auto queue = std::make_shared<QueueT>(json_files.size());

  std::size_t processed_events = 0;
  bool have_timestamps = false;
  std::uint64_t first_ts_event = 0;
  std::uint64_t last_ts_event = 0;

  std::queue<domain::events::MarketDataEvent> first_ten;
  std::queue<domain::events::MarketDataEvent> last_ten;

  const auto started_at = std::chrono::steady_clock::now();
  {
    data_layer::MarketDataFolderLoaderT<QueueT> folder_loader(json_files,
                                                              queue);
    transport::QueueSubscriberT<QueueT> subscriber(queue);

    subscriber.addSubscriber({
        .name = "log sub",
        .onEvent =
            [&](const domain::events::MarketDataEvent &event) {
              if (emit_event_logs) {
                processMarketDataEvent(event);
              }
              ++processed_events;
              if (!have_timestamps) {
                first_ts_event = event.hd.ts_event;
                have_timestamps = true;
              }
              last_ts_event = event.hd.ts_event;
              if (!hard_mode) {
                if (first_ten.size() < 10) {
                  first_ten.push(event);
                }
                if (last_ten.size() >= 10) {
                  last_ten.pop();
                }
                last_ten.push(event);
              }
            },
        .onEndEvents =
            [&]() {
              if (!hard_mode) {
                dumpFirstLastTen(first_ten, last_ten);
              }
            },
    });

    common::pipeline::runPipeline(folder_loader, subscriber);
    common::pipeline::stopPipeline(subscriber, folder_loader);
  }

  const auto ended_at = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration<double>(ended_at - started_at);

  std::cout << "\n=== " << name << " — stream summary ===\n";
  std::cout << "Total messages: " << processed_events << '\n';
  if (have_timestamps) {
    std::cout << "First ts_event (ns): " << first_ts_event << '\n';
    std::cout << "Last ts_event (ns): " << last_ts_event << '\n';
  } else {
    std::cout << "First/last ts_event: (no events)\n";
  }

  return {
      .name = name,
      .processed_events = processed_events,
      .elapsed_seconds = elapsed.count(),
      .have_timestamps = have_timestamps,
      .first_ts_event = first_ts_event,
      .last_ts_event = last_ts_event,
  };
}

void printComparison(const Stats &flat, const Stats &hierarchical) {
  const auto throughput = [](const Stats &stats) {
    return stats.elapsed_seconds > 0.0
               ? static_cast<double>(stats.processed_events) /
                     stats.elapsed_seconds
               : 0.0;
  };

  const auto flat_throughput = throughput(flat);
  const auto hierarchical_throughput = throughput(hierarchical);

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "\n=== Merge Comparison ===\n";
  std::cout << flat.name << ": events=" << flat.processed_events
            << ", elapsed=" << flat.elapsed_seconds
            << " s, throughput=" << flat_throughput << " events/s\n";
  if (flat.have_timestamps) {
    std::cout << "  first_ts_event_ns=" << flat.first_ts_event
              << " last_ts_event_ns=" << flat.last_ts_event << '\n';
  }
  std::cout << hierarchical.name << ": events=" << hierarchical.processed_events
            << ", elapsed=" << hierarchical.elapsed_seconds
            << " s, throughput=" << hierarchical_throughput << " events/s\n";
  if (hierarchical.have_timestamps) {
    std::cout << "  first_ts_event_ns=" << hierarchical.first_ts_event
              << " last_ts_event_ns=" << hierarchical.last_ts_event << '\n';
  }
}

} // namespace

int main(int argc, char *argv[]) {
  namespace fs = std::filesystem;

  constexpr std::string_view suppress_logs_arg = "--suppress-logs";

  if (argc < 2 || argc > 3) {
    std::cerr
        << "Usage: " << argv[0]
        << " <folder_with_mbo_json | path/to/file.mbo.json> [--suppress-logs]\n"
        << "  file: print first/last 10 events; folder: print every event\n"
        << "  --suppress-logs: folder mode only; skip per-event printing "
           "(benchmark)\n";
    return 1;
  }

  std::string path;
  bool suppress_logs = false;
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg == suppress_logs_arg) {
      suppress_logs = true;
    } else if (path.empty()) {
      path.assign(arg.data(), arg.size());
    } else {
      std::cerr << "Unexpected argument: " << arg << '\n';
      return 1;
    }
  }

  if (path.empty()) {
    std::cerr << "Missing path argument\n";
    return 1;
  }

  std::vector<std::string> json_files;
  bool hard_mode = false;

  try {
    if (fs::is_directory(path)) {
      json_files = data_layer::discoverJsonFiles(path);
      hard_mode = true;
    } else if (fs::is_regular_file(path)) {
      if (!path.ends_with(".json")) {
        std::cerr << "Expected a .json file or a directory\n";
        return 1;
      }
      json_files = {path};
      hard_mode = false;
    } else {
      std::cerr << "Not a file or directory: " << path << '\n';
      return 1;
    }
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  if (suppress_logs && !hard_mode) {
    std::cerr << "--suppress-logs is only valid for a directory (hard mode)\n";
    return 1;
  }

  if (json_files.empty()) {
    std::cerr << "No .mbo.json files to process\n";
    return 1;
  }

  const bool emit_event_logs = hard_mode && !suppress_logs;

  const auto flat_stats = runPipelineBenchmark<transport::FlatSyncedQueue>(
      "Flat merge", json_files, hard_mode, emit_event_logs);

  const auto hierarchical_stats =
      runPipelineBenchmark<transport::HierarchicalSyncedQueue>(
          "Hierarchical merge", json_files, hard_mode, emit_event_logs);

  printComparison(flat_stats, hierarchical_stats);

  return 0;
}
