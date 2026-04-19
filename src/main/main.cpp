// main function for the back-tester app
// please, keep it minimalistic

#include "common/MarketDataEvent.hpp"
#include "ingest/FolderIngest.hpp"
#include "ingest/NdjsonReader.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
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
};

struct PreviewState {
  explicit PreviewState(std::size_t preview_limit)
      : preview_limit(preview_limit),
        tail(preview_limit == 0 ? 1 : preview_limit) {}

  std::size_t preview_limit = 0;
  std::vector<MarketDataEvent> head;
  std::vector<MarketDataEvent> tail;
  std::size_t tailCount = 0;
  std::size_t tailStart = 0;
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

    if (tailCount < preview_limit) {
      tail[tailCount] = event;
      ++tailCount;
    } else {
      tail[tailStart] = event;
      tailStart = (tailStart + 1) % preview_limit;
    }
  }

  void printTail() const {
    if (preview_limit == 0) {
      return;
    }
    const std::size_t tailToPrint =
        consumed > preview_limit
            ? std::min(preview_limit, consumed - preview_limit)
            : 0;
    if (tailToPrint == 0) {
      return;
    }
    std::cout << "\n--- tail ---\n";
    const std::size_t tailSkip = tailCount - tailToPrint;
    for (std::size_t i = 0; i < tailToPrint; ++i) {
      const std::size_t idx = (tailStart + tailSkip + i) % preview_limit;
      std::cout << tail[idx] << '\n';
    }
  }
};

PreviewState *gPreview = nullptr;

[[noreturn]] void usageError(std::string_view message) {
  throw std::runtime_error(
      std::string(message) +
      "\nusage: back-tester <path> [--strategy flat|hierarchy|both] "
      "[--preview-limit N]");
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

std::size_t parsePreviewLimit(std::string_view value) {
  std::size_t parsed = 0;
  const char *first = value.data();
  const char *last = first + value.size();
  const auto [ptr, ec] = std::from_chars(first, last, parsed);
  if (ec != std::errc() || ptr != last) {
    usageError("invalid preview limit");
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
      options.preview_limit = parsePreviewLimit(argv[++i]);
      continue;
    }
    usageError("unknown argument");
  }

  return options;
}

void printSingleFileSummary(const IngestStats &stats) {
  std::cout << "\n"
            << "total=" << stats.consumed
            << " skipped_rtype=" << stats.skipped_rtype
            << " skipped_parse=" << stats.skipped_parse
            << " first_ts_recv=" << stats.first_ts_recv
            << " last_ts_recv=" << stats.last_ts_recv
            << " out_of_order_ts_recv=" << stats.out_of_order_ts_recv << '\n';
}

void runSingleFile(const std::filesystem::path &path, std::size_t preview_limit) {
  PreviewState preview(preview_limit);
  gPreview = &preview;
  const IngestStats stats = parseNdjsonFile(path, &processMarketDataEvent);
  gPreview = nullptr;
  preview.printTail();
  printSingleFileSummary(stats);
}

void printFolderSummary(const FolderIngestStats &stats) {
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
            << " msgs_per_sec=" << stats.msgsPerSec() << '\n';
}

void runFolderStrategy(const std::filesystem::path &path, MergeStrategy strategy,
                       std::size_t preview_limit) {
  std::cout << "--- strategy=" << mergeStrategyName(strategy) << " ---\n";
  PreviewState preview(preview_limit);
  gPreview = &preview;
  const FolderIngestStats stats = ingestFolder(path, strategy, &processMarketDataEvent);
  gPreview = nullptr;
  preview.printTail();
  printFolderSummary(stats);
}

} // namespace

namespace cmf {

// Spec-named consumer seam. Called for every decoded event; prints the first
// preview-limit events directly, and keeps the latest preview-limit in a ring
// buffer for main() to flush after the stream ends.
void processMarketDataEvent(const MarketDataEvent &event) {
  if (gPreview != nullptr) {
    gPreview->consume(event);
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
        runFolderStrategy(options.input_path, MergeStrategy::Flat,
                          options.preview_limit);
        break;
      case StrategySelection::Hierarchy:
        runFolderStrategy(options.input_path, MergeStrategy::Hierarchy,
                          options.preview_limit);
        break;
      case StrategySelection::Both:
        runFolderStrategy(options.input_path, MergeStrategy::Flat,
                          options.preview_limit);
        std::cout << '\n';
        runFolderStrategy(options.input_path, MergeStrategy::Hierarchy,
                          options.preview_limit);
        break;
      case StrategySelection::Default:
        break;
      }
    } else {
      if (options.strategy != StrategySelection::Default) {
        usageError("--strategy only valid for directory input");
      }
      runSingleFile(options.input_path, options.preview_limit);
    }
  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
