// main function for the back-tester app
// please, keep it minimalistic

#include "common/IngestionPipeline.hpp"
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

int main(int argc, const char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: " << argv[0] << " <folder_path>" << std::endl;
      std::cerr << "  folder_path: Directory containing NDJSON market data "
                   "files"
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

    // Run optimized multi-threaded pipeline
    auto start = std::chrono::high_resolution_clock::now();

    using MergerT = FlatMerger<EventComparator>;
    IngestionPipeline<MergerT> pipeline(filepaths);
    pipeline.run(processMarketDataEvent);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double seconds = duration.count() / 1000.0;
    size_t total_messages = pipeline.getTotalMessages();
    double throughput = total_messages / seconds;

    // Print summary
    std::cout << "Total messages processed : " << total_messages << std::endl;
    std::cout << "Wall-clock time          : " << seconds << " s" << std::endl;
    std::cout << "Throughput               : " << static_cast<long>(throughput)
              << " msg/s" << std::endl;
    std::cout << "\nSummary:" << std::endl;
    std::cout << "  Files processed        : " << filepaths.size() << std::endl;
    std::cout << "  Producer threads       : " << filepaths.size() << std::endl;
    std::cout << "  Parser                 : NativeDataParser (optimized)"
              << std::endl;
    std::cout << "  Queue type             : Lock-free SPSC" << std::endl;
    std::cout << "  Merge strategy         : Flat k-way merge" << std::endl;

  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
