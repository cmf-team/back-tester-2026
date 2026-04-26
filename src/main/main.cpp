// main function for the back-tester app
// please, keep it minimalistic

#include "common/MarketDataEvent.hpp"
#include "ingest/FolderIngest.hpp"
#include "ingest/NdjsonReader.hpp"
#include "lob/BookDispatcher.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace cmf;

namespace cmf {
void processMarketDataEvent(const MarketDataEvent &event);
}

namespace {

constexpr std::size_t kDefaultPreviewSize = 10;

enum class StrategySelection {
  Default,
  Flat,
  Hierarchy,
  Both,
};

struct CliOptions {
  std::filesystem::path input_path;
  StrategySelection strategy = StrategySelection::Default;
  std::size_t preview_limit = kDefaultPreviewSize;
  std::size_t snapshot_every = 100'000;
  std::size_t snapshot_depth = 5;
  std::size_t max_snapshots = 3;
  std::size_t final_books_limit = 20;
};

struct PreviewState {
  explicit PreviewState(std::size_t preview_limit)
      : preview_limit(preview_limit),
        tail(preview_limit == 0 ? 1 : preview_limit) {}

  std::size_t preview_limit = 0;
  std::vector<MarketDataEvent> head;
  std::vector<MarketDataEvent> tail;
  std::size_t tail_count = 0;
  std::size_t tail_start = 0;
  std::size_t consumed = 0;

  void consume(const MarketDataEvent &event) {
    ++consumed;
    if (preview_limit == 0) {
      return;
    }
    if (head.size() < preview_limit) {
      head.push_back(event);
      std::cout << event << '\n';
    }

    if (tail_count < preview_limit) {
      tail[tail_count] = event;
      ++tail_count;
    } else {
      tail[tail_start] = event;
      tail_start = (tail_start + 1) % preview_limit;
    }
  }

  void printTail() const {
    if (preview_limit == 0) {
      return;
    }
    const std::size_t tail_to_print =
        consumed > preview_limit
            ? std::min(preview_limit, consumed - preview_limit)
            : 0;
    if (tail_to_print == 0) {
      return;
    }
    std::cout << "\n--- tail ---\n";
    const std::size_t tail_skip = tail_count - tail_to_print;
    for (std::size_t i = 0; i < tail_to_print; ++i) {
      const std::size_t idx = (tail_start + tail_skip + i) % preview_limit;
      std::cout << tail[idx] << '\n';
    }
  }
};

struct ReplayState {
  PreviewState *preview = nullptr;
  BookDispatcher *dispatcher = nullptr;

  void consume(const MarketDataEvent &event) const {
    if (preview != nullptr) {
      preview->consume(event);
    }
    if (dispatcher != nullptr) {
      dispatcher->apply(event);
    }
  }
};

ReplayState *gReplay = nullptr;

[[noreturn]] void usageError(std::string_view message) {
  throw std::runtime_error(
      std::string(message) +
      "\nusage: back-tester <path> [--strategy flat|hierarchy|both] "
      "[--preview-limit N] [--snapshot-every N] [--snapshot-depth N] "
      "[--max-snapshots N] [--final-books-limit N]");
}

StrategySelection parseStrategySelection(std::string_view value) {
  if (value == "flat") {
    return StrategySelection::Flat;
  }
  if (value == "hierarchy") {
    return StrategySelection::Hierarchy;
  }
  if (value == "both") {
    return StrategySelection::Both;
  }
  usageError("invalid strategy");
}

std::size_t parseCount(std::string_view value, std::string_view name) {
  std::size_t parsed = 0;
  const char *first = value.data();
  const char *last = first + value.size();
  const auto [ptr, ec] = std::from_chars(first, last, parsed);
  if (ec != std::errc() || ptr != last) {
    usageError(std::string("invalid ") + std::string(name));
  }
  return parsed;
}

CliOptions parseCli(int argc, const char *argv[]) {
  if (argc < 2) {
    usageError("missing input path");
  }

  CliOptions options{};
  options.input_path = argv[1];

  for (int i = 2; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--strategy") {
      if (i + 1 >= argc) {
        usageError("missing value for --strategy");
      }
      options.strategy = parseStrategySelection(argv[++i]);
      continue;
    }
    if (arg == "--preview-limit") {
      if (i + 1 >= argc) {
        usageError("missing value for --preview-limit");
      }
      options.preview_limit = parseCount(argv[++i], "preview limit");
      continue;
    }
    if (arg == "--snapshot-every") {
      if (i + 1 >= argc) {
        usageError("missing value for --snapshot-every");
      }
      options.snapshot_every = parseCount(argv[++i], "snapshot interval");
      continue;
    }
    if (arg == "--snapshot-depth") {
      if (i + 1 >= argc) {
        usageError("missing value for --snapshot-depth");
      }
      options.snapshot_depth = parseCount(argv[++i], "snapshot depth");
      continue;
    }
    if (arg == "--max-snapshots") {
      if (i + 1 >= argc) {
        usageError("missing value for --max-snapshots");
      }
      options.max_snapshots = parseCount(argv[++i], "max snapshots");
      continue;
    }
    if (arg == "--final-books-limit") {
      if (i + 1 >= argc) {
        usageError("missing value for --final-books-limit");
      }
      options.final_books_limit = parseCount(argv[++i], "final books limit");
      continue;
    }
    usageError("unknown argument");
  }

  return options;
}

DispatchOptions toDispatchOptions(const CliOptions &options) {
  return DispatchOptions{
      .snapshot_every = options.snapshot_every,
      .max_snapshots = options.max_snapshots,
      .snapshot_depth = options.snapshot_depth,
  };
}

void printBookSnapshots(const BookDispatcher &dispatcher) {
  if (dispatcher.snapshots().empty()) {
    return;
  }
  std::cout << "\n--- lob snapshots ---\n";
  for (const auto &snapshot : dispatcher.snapshots()) {
    std::cout << "event=" << snapshot.event_index
              << " ts_recv=" << snapshot.ts_recv << ' '
              << snapshot.book.snapshot << '\n';
  }
}

void printFinalBooks(const BookDispatcher &dispatcher, std::size_t limit) {
  const auto summaries = dispatcher.finalSummaries(1);
  if (summaries.empty()) {
    return;
  }
  const std::size_t to_print =
      limit == 0 ? summaries.size() : std::min(limit, summaries.size());

  std::cout << "\n--- final best bid/ask ---\n";
  for (std::size_t i = 0; i < to_print; ++i) {
    std::cout << summaries[i].snapshot << '\n';
  }
  if (to_print < summaries.size()) {
    std::cout << "... truncated " << (summaries.size() - to_print)
              << " instruments\n";
  }
}

void printDispatcherSummary(const DispatchStats &stats) {
  std::cout << " instruments=" << stats.instruments
            << " snapshots=" << stats.snapshots
            << " missing_order_events=" << stats.missing_order_events
            << " ignored_events=" << stats.ignored_events
            << " unresolved_routes=" << stats.unresolved_routes
            << " ambiguous_routes=" << stats.ambiguous_routes
            << " adds=" << stats.adds << " cancels=" << stats.cancels
            << " modifies=" << stats.modifies << " clears=" << stats.clears
            << " trades=" << stats.trades << " fills=" << stats.fills
            << " none=" << stats.none;
}

void printSingleFileSummary(const IngestStats &stats,
                            const DispatchStats &dispatch,
                            double elapsed_sec) {
  std::cout << "\n"
            << "total=" << stats.consumed
            << " skipped_rtype=" << stats.skipped_rtype
            << " skipped_parse=" << stats.skipped_parse
            << " first_ts_recv=" << stats.first_ts_recv
            << " last_ts_recv=" << stats.last_ts_recv
            << " out_of_order_ts_recv=" << stats.out_of_order_ts_recv
            << " elapsed_sec=" << elapsed_sec
            << " msgs_per_sec="
            << (elapsed_sec > 0.0 ? static_cast<double>(stats.consumed) /
                                        elapsed_sec
                                  : 0.0);
  printDispatcherSummary(dispatch);
  std::cout << '\n';
}

void printFolderSummary(const FolderIngestStats &stats,
                        const DispatchStats &dispatch) {
  std::cout << "\n"
            << "strategy=" << mergeStrategyName(stats.strategy)
            << " files=" << stats.files << " total=" << stats.total
            << " skipped_rtype=" << stats.skipped_rtype
            << " skipped_parse=" << stats.skipped_parse
            << " first_ts_recv=" << stats.first_ts_recv
            << " last_ts_recv=" << stats.last_ts_recv
            << " out_of_order_ts_recv=" << stats.out_of_order_ts_recv
            << " producer_out_of_order_ts_recv="
            << stats.producer_out_of_order_ts_recv
            << " elapsed_sec=" << stats.elapsed_sec
            << " msgs_per_sec=" << stats.msgsPerSec();
  printDispatcherSummary(dispatch);
  std::cout << '\n';
}

void runSingleFile(const std::filesystem::path &path, const CliOptions &options) {
  PreviewState preview(options.preview_limit);
  BookDispatcher dispatcher(toDispatchOptions(options));
  ReplayState replay{.preview = &preview, .dispatcher = &dispatcher};
  gReplay = &replay;

  const auto started = std::chrono::steady_clock::now();
  const IngestStats stats = parseNdjsonFile(path, &processMarketDataEvent);
  const auto finished = std::chrono::steady_clock::now();
  gReplay = nullptr;

  const double elapsed_sec =
      std::chrono::duration<double>(finished - started).count();
  preview.printTail();
  printBookSnapshots(dispatcher);
  printFinalBooks(dispatcher, options.final_books_limit);
  printSingleFileSummary(stats, dispatcher.stats(), elapsed_sec);
}

void runFolderStrategy(const std::filesystem::path &path, MergeStrategy strategy,
                       const CliOptions &options) {
  std::cout << "--- strategy=" << mergeStrategyName(strategy) << " ---\n";
  PreviewState preview(options.preview_limit);
  BookDispatcher dispatcher(toDispatchOptions(options));
  ReplayState replay{.preview = &preview, .dispatcher = &dispatcher};
  gReplay = &replay;

  const FolderIngestStats stats =
      ingestFolder(path, strategy, &processMarketDataEvent);
  gReplay = nullptr;

  preview.printTail();
  printBookSnapshots(dispatcher);
  printFinalBooks(dispatcher, options.final_books_limit);
  printFolderSummary(stats, dispatcher.stats());
}

} // namespace

namespace cmf {

// Spec-named consumer seam. Called for every decoded event; keeps the legacy
// raw-event preview and now also drives the per-instrument LOB dispatcher.
void processMarketDataEvent(const MarketDataEvent &event) {
  if (gReplay != nullptr) {
    gReplay->consume(event);
  }
}

} // namespace cmf

int main(int argc, const char *argv[]) {
  try {
    const CliOptions options = parseCli(argc, argv);
    if (std::filesystem::is_directory(options.input_path)) {
      const StrategySelection selection =
          options.strategy == StrategySelection::Default
              ? StrategySelection::Both
              : options.strategy;
      switch (selection) {
      case StrategySelection::Flat:
        runFolderStrategy(options.input_path, MergeStrategy::Flat, options);
        break;
      case StrategySelection::Hierarchy:
        runFolderStrategy(options.input_path, MergeStrategy::Hierarchy, options);
        break;
      case StrategySelection::Both:
        runFolderStrategy(options.input_path, MergeStrategy::Flat, options);
        std::cout << '\n';
        runFolderStrategy(options.input_path, MergeStrategy::Hierarchy, options);
        break;
      case StrategySelection::Default:
        break;
      }
    } else {
      if (options.strategy != StrategySelection::Default) {
        usageError("--strategy only valid for directory input");
      }
      runSingleFile(options.input_path, options);
    }
  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
