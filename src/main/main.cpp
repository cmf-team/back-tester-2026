// main function for the back-tester app
// please, keep it minimalistic

#include "common/IngestionPipeline.hpp"
#include "common/MarketDataEvent.hpp"
#include "common/MergeStrategies.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace cmf;
using namespace cmf::EventParser;
namespace fs = std::filesystem;

void processMarketDataEvent([[maybe_unused]] const MarketDataEvent &event) {
  // This function will later update the Limit Order Book (LOB)
  // For now, it just counts events
}

void runSimpleBenchmark(const char *strategy_name,
                        const std::vector<std::string> &filepaths,
                        bool use_hierarchy) {
  std::cout << "\n=== Running " << strategy_name << " (Simple) ==="
            << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  size_t total_messages = 0;

  if (use_hierarchy) {
    HierarchyMerger<> merger(filepaths);
    while (merger.hasNext()) {
      MarketDataEvent event = merger.getNext();
      processMarketDataEvent(event);
      total_messages++;
    }
  } else {
    FlatMerger<> merger(filepaths);
    while (merger.hasNext()) {
      MarketDataEvent event = merger.getNext();
      processMarketDataEvent(event);
      total_messages++;
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  double seconds = duration.count() / 1000.0;
  double throughput = total_messages / seconds;

  std::cout << "Total messages processed : " << total_messages << std::endl;
  std::cout << "Wall-clock time          : " << seconds << " s" << std::endl;
  std::cout << "Throughput               : " << static_cast<int>(throughput)
            << " msg/s" << std::endl;
}

void runOptimizedBenchmark(const std::vector<std::string> &filepaths) {
  std::cout << "\n=== Running Optimized Multi-threaded Pipeline ==="
            << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  using MergerT = FlatMerger<EventComparator>;
  IngestionPipeline<MergerT> pipeline_obj(filepaths);
  pipeline_obj.run(processMarketDataEvent);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  double seconds = duration.count() / 1000.0;
  double throughput = pipeline_obj.getTotalMessages() / seconds;

  std::cout << "Total messages processed : " << pipeline_obj.getTotalMessages()
            << std::endl;
  std::cout << "Wall-clock time          : " << seconds << " s" << std::endl;
  std::cout << "Throughput               : " << static_cast<int>(throughput)
            << " msg/s" << std::endl;
}

void runBatchBenchmark(const std::vector<std::string> &filepaths,
                       size_t batch_size = 1024) {
  std::cout << "\n=== Running Batch Processing (batch=" << batch_size
            << ") ===" << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  using MergerT = FlatMerger<EventComparator>;
  BatchIngestionPipeline<MergerT> pipeline_obj(filepaths, batch_size);
  size_t total = pipeline_obj.run(processMarketDataEvent);

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  double seconds = duration.count() / 1000.0;
  double throughput = total / seconds;

  std::cout << "Total messages processed : " << total << std::endl;
  std::cout << "Wall-clock time          : " << seconds << " s" << std::endl;
  std::cout << "Throughput               : " << static_cast<int>(throughput)
            << " msg/s" << std::endl;
}

int main(int argc, const char *argv[]) {
  try {
    bool run_optimized = false;
    bool run_batch = false;
    bool run_all = false;
    std::string folder_path;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
      if (std::strcmp(argv[i], "--optimized") == 0) {
        run_optimized = true;
      } else if (std::strcmp(argv[i], "--batch") == 0) {
        run_batch = true;
      } else if (std::strcmp(argv[i], "--all") == 0) {
        run_all = true;
      } else if (folder_path.empty()) {
        folder_path = argv[i];
      }
    }

    if (folder_path.empty()) {
      std::cerr << "Usage: " << argv[0]
                << " <folder_path> [--optimized] [--batch] [--all]"
                << std::endl;
      std::cerr << "  folder_path: Directory containing NDJSON market data "
                   "files"
                << std::endl;
      std::cerr << "  --optimized: Run multi-threaded optimized pipeline"
                << std::endl;
      std::cerr << "  --batch: Run batch processing mode" << std::endl;
      std::cerr << "  --all: Run all benchmarks" << std::endl;
      return 1;
    }

    if (!fs::exists(folder_path) || !fs::is_directory(folder_path)) {
      std::cerr << "Error: " << folder_path << " is not a valid directory"
                << std::endl;
      return 1;
    }

    // Collect all .mbo.json files (market data files)
    std::vector<std::string> filepaths;
    for (const auto &entry : fs::directory_iterator(folder_path)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        // Only process market data files, skip metadata/manifest/condition
        // files
        if (filename.find(".mbo.json") != std::string::npos) {
          filepaths.push_back(entry.path().string());
        }
      }
    }

    if (filepaths.empty()) {
      std::cerr << "Error: No market data files found in " << folder_path
                << std::endl;
      return 1;
    }

    // Sort filepaths for consistent ordering
    std::sort(filepaths.begin(), filepaths.end());

    std::cout << "Found " << filepaths.size() << " market data files"
              << std::endl;

    // Run benchmarks based on flags
    if (run_all || (!run_optimized && !run_batch)) {
      // Run simple benchmarks
      runSimpleBenchmark("Flat Merger", filepaths, false);
      runSimpleBenchmark("Hierarchy Merger", filepaths, true);
    }

    if (run_all || run_optimized) {
      runOptimizedBenchmark(filepaths);
    }

    if (run_all || run_batch) {
      runBatchBenchmark(filepaths, 1024);
      runBatchBenchmark(filepaths, 4096);
    }

  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
