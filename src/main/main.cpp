// main function for the back-tester app
// please, keep it minimalistic

#include "common/MarketDataEvent.hpp"
#include "common/MergeStrategies.hpp"
#include <algorithm>
#include <chrono>
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

void runBenchmark(const char *strategy_name,
                  const std::vector<std::string> &filepaths,
                  bool use_hierarchy) {
  std::cout << "\n=== Running " << strategy_name << " ===" << std::endl;

  auto start = std::chrono::high_resolution_clock::now();

  size_t total_messages = 0;

  if (use_hierarchy) {
    HierarchyMerger merger(filepaths);
    while (merger.hasNext()) {
      MarketDataEvent event = merger.getNext();
      processMarketDataEvent(event);
      total_messages++;
    }
  } else {
    FlatMerger merger(filepaths);
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

  std::cout << "Total messages processed: " << total_messages << std::endl;
  std::cout << "Wall-clock time: " << seconds << " seconds" << std::endl;
  std::cout << "Throughput: " << throughput << " messages/second" << std::endl;
}

int main(int argc, const char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: " << argv[0] << " <folder_path>" << std::endl;
      std::cerr << "  folder_path: Directory containing NDJSON market data files"
                << std::endl;
      return 1;
    }

    const std::string folder_path = argv[1];

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

    // Run benchmarks for both merge strategies
    runBenchmark("Flat Merger", filepaths, false);
    runBenchmark("Hierarchy Merger", filepaths, true);

  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
