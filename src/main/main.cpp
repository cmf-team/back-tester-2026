// main function for the back-tester app
// please, keep it minimalistic
#include "LineReader.hpp"
#include "ThreadSafeQueue.hpp"
#include "EventParser.hpp"
#include "Merger.hpp"
#include "common/BasicTypes.hpp"
#include "MarketDataEvent.hpp"
#include <iostream>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <thread>

namespace fs = std::filesystem;
using namespace cmf;

void produce(const std::string& filePath, ThreadSafeQueue<MarketDataEvent>& queue) {
  LineReader reader(filePath);
  std::string line;

  while (reader.nextLine(line)) {
    try {
      MarketDataEvent event = parseEvent(line);
      queue.push(event);
    } catch (const std::exception& e) {
      // skip malformed lines
    }
  }
  queue.setDone();
}

void processMarketDataEvent([[maybe_unused]] const MarketDataEvent& event) {

}

// collect .mbo.json files from a directory
std::vector<std::string> collectFiles(const std::string& dirPath) {
  std::vector<std::string> files;
  for (const auto& entry : fs::directory_iterator(dirPath)) {
    if (!entry.is_regular_file()) continue;
    std::string filename = entry.path().filename().string();
    if (filename.size() > 9 && filename.substr(filename.size() - 9) == ".mbo.json") {
      files.push_back(entry.path().string());
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

// launch producer threads, run a merger, join threads, return count + elapsed
void runBenchmark(
    const std::string& name,
    const std::vector<std::string>& files,
    std::function<size_t(std::vector<ThreadSafeQueue<MarketDataEvent>>&,
                         std::function<void(const MarketDataEvent&)>)> merger
) {
  size_t n = files.size();
  std::vector<ThreadSafeQueue<MarketDataEvent>> queues(n);
  std::vector<std::thread> threads;

  auto start = std::chrono::steady_clock::now();


  for (size_t i = 0; i < n; ++i) {
    threads.emplace_back(produce, files[i], std::ref(queues[i]));
  }


  size_t count = merger(queues, processMarketDataEvent);


  for (auto& t : threads) {
    t.join();
  }

  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(end - start).count();

  std::cout << "=== " << name << " ===" << '\n';
  std::cout << "Total messages: " << count << '\n';
  std::cout << "Wall-clock time: " << elapsed << " sec" << '\n';
  std::cout << "Throughput: " << static_cast<size_t>(count / elapsed) << " msg/sec" << '\n';
  std::cout << '\n';
}

int main([[maybe_unused]] int argc, [[maybe_unused]] const char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: back-tester <path-to-folder>\n";
    return 1;
  }

  std::string dirPath = argv[1];
  std::vector<std::string> files = collectFiles(dirPath);

  std::cout << "Found " << files.size() << " files\n\n";

  // Benchmark 1: Flat Merger
  runBenchmark("Flat Merger", files, flatMerge);

  // Benchmark 2: Hierarchy Merger
  runBenchmark("Hierarchy Merger", files, hierarchyMerge);

  return 0;
}
