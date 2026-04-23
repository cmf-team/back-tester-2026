#include "MarketDataEvent.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>

using json = nlohmann::json;

void processMarketDataEvent(const MarketDataEvent& event) {
    std::cout << "ts=" << event.timestamp
              << " order_id=" << event.order_id
              << " side=" << event.side
              << " price=" << event.price
              << " size=" << event.size
              << " action=" << event.action
              << "\n";
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path-to-ndjson-file>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Error: cannot open file " << argv[1] << "\n";
        return 1;
    }

    std::deque<MarketDataEvent> first10;
    std::deque<MarketDataEvent> last10;
    uint64_t totalMessages = 0;
    std::string firstTimestamp;
    std::string lastTimestamp;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        json j;
        try {
            j = json::parse(line);
        } catch (const json::parse_error&) {
            continue;
        }

        MarketDataEvent event;
        event.timestamp = j["hd"]["ts_event"].get<std::string>();
        event.order_id = j["order_id"].get<std::string>();

        std::string sideStr = j["side"].get<std::string>();
        event.side = sideStr.empty() ? 'N' : sideStr[0];

        if (j["price"].is_null()) {
            event.price = 0.0;
        } else {
            event.price = std::stod(j["price"].get<std::string>());
        }

        event.size = j["size"].get<uint32_t>();

        std::string actionStr = j["action"].get<std::string>();
        event.action = actionStr.empty() ? '?' : actionStr[0];

        event.instrument_id = j["hd"]["instrument_id"].get<uint32_t>();
        event.symbol = j["symbol"].get<std::string>();

        if (totalMessages == 0) {
            firstTimestamp = event.timestamp;
        }
        lastTimestamp = event.timestamp;
        ++totalMessages;

        if (first10.size() < 10) {
            first10.push_back(event);
        }
        if (last10.size() == 10) {
            last10.pop_front();
        }
        last10.push_back(event);
    }

    std::cout << "=== First 10 events ===\n";
    for (const auto& e : first10) {
        processMarketDataEvent(e);
    }

    std::cout << "\n=== Last 10 events ===\n";
    for (const auto& e : last10) {
        processMarketDataEvent(e);
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Total messages processed: " << totalMessages << "\n";
    std::cout << "First timestamp: " << firstTimestamp << "\n";
    std::cout << "Last timestamp:  " << lastTimestamp << "\n";

    return 0;
}
