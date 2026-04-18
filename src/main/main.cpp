// main function for the back-tester app
// please, keep it minimalistic

#include "common/MarketDataEvent.hpp"
#include "main/ingestion/MarketDataParser.hpp"

#include <deque>
#include <iostream>
#include <string>

using namespace cmf;

void processMarketDataEvent(const MarketDataEvent &event) {
  std::cout << event << "\n";
}

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: back-tester <path-to-daily-ndjson-file>\n";
    return 1;
  }

  try {
    const std::string filePath = argv[1];

    NanoTime firstTs = 0;
    NanoTime lastTs = 0;
    std::size_t totalCount = 0;

    std::deque<MarketDataEvent> first10;
    std::deque<MarketDataEvent> last10;

    MarketDataParser parser;
    totalCount = parser.parseFile(filePath, [&](const MarketDataEvent &event) {
      if (first10.size() < 10) {
        first10.push_back(event);
      }
      if (last10.size() == 10) {
        last10.pop_front();
      }
      last10.push_back(event);

      if (firstTs == 0) {
        firstTs = event.tsRecv;
      }
      lastTs = event.tsRecv;
    });

    std::cout << "=== First 10 events ===\n";
    for (const auto &e : first10) {
      processMarketDataEvent(e);
    }

    std::cout << "\n=== Last 10 events ===\n";
    for (const auto &e : last10) {
      processMarketDataEvent(e);
    }

    std::cout << "\n=== Summary ===\n"
              << "Total messages processed: " << totalCount << "\n"
              << "First timestamp: " << firstTs << "\n"
              << "Last timestamp:  " << lastTs << "\n";

  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
