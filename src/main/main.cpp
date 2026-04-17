// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"
#include "common/MarketDataEvent.hpp"
#include "common/MarketDataParser.hpp"
#include <deque>
#include <fstream>
#include <iostream>
#include <vector>

using namespace cmf;

void processMarketDataEvent(const MarketDataEvent &event) {
  std::cout << "sort_ts=" << event.getSortTs()
            << ", ts_event=" << event.getTsEvent()
            << ", order_id=" << event.getOrderId()
            << ", instrument_id=" << event.getInstrumentId()
            << ", side=" << MarketDataEvent::sideToString(event.getSide())
            << ", price=" << event.getPrice() << ", size=" << event.getSize()
            << ", action=" << MarketDataEvent::actionToString(event.getAction())
            << ", flags=" << static_cast<unsigned>(event.getFlags()) << '\n';
}
#include <iostream>

int main(int argc, const char *argv[]) {
  // 1. Argument Check
  if (argc < 2) {
    std::cerr << "Error: No file path provided.\n";
    std::cerr << "Usage: ./backtester <path_to_json_file>\n";
    return 1;
  }

  const std::string file_path = argv[1];
  std::ifstream file(file_path);
  if (!file.is_open()) {
    std::cerr << "Error: Could not open file " << file_path << "\n";
    return 1;
  }

  // 2. State for Summary
  std::size_t count = 0;
  uint64_t first_ts = 0, last_ts = 0;
  std::vector<MarketDataEvent> first_10;
  std::deque<MarketDataEvent> last_10;

  // 3. Line-by-line Processing
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty())
      continue;

    auto event_opt = parseLine(line);
    if (!event_opt)
      continue;

    const auto &ev = *event_opt;

    // Collect stats
    if (count == 0)
      first_ts = ev.getSortTs();
    last_ts = ev.getSortTs();

    if (first_10.size() < 10)
      first_10.push_back(ev);
    last_10.push_back(ev);
    if (last_10.size() > 10)
      last_10.pop_front();

    // Verification Consumer
    // processMarketDataEvent(ev); // Uncomment to see every message

    count++;
  }

  // 4. Verification Output
  std::cout << "\n--- Objective 1 Verification ---\n";
  std::cout << "First 10 Events:\n";
  for (const auto &e : first_10) {
    std::cout << "  TS: " << e.getSortTs() << " | ID: " << e.getOrderId()
              << " | Price: " << e.getPrice() << "\n";
  }

  std::cout << "\nLast 10 Events:\n";
  for (const auto &e : last_10) {
    std::cout << "  TS: " << e.getSortTs() << " | ID: " << e.getOrderId()
              << " | Price: " << e.getPrice() << "\n";
  }

  std::cout << "\nSummary:\n";
  std::cout << "  Total Messages:  " << count << "\n";
  std::cout << "  Start Timestamp: " << first_ts << " ns\n";
  std::cout << "  End Timestamp:   " << last_ts << " ns\n";

    return 0;
}