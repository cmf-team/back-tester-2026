#include "JsonParser.hpp"

#include <deque>
#include <fstream>
#include <iostream>

using namespace cmf;

static void processMarketDataEvent(const MarketDataEvent& e) {
    std::cout
        << "ts=" << e.timestamp
        << " orderId=" << e.orderId
        << " side=" << static_cast<int>(e.side)
        << " price=" << e.price
        << " size=" << e.size
        << " action=" << static_cast<int>(e.action)
        << "\n";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ingestion_app <file>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Cannot open file\n";
        return 1;
    }

    std::string line;

    std::deque<MarketDataEvent> first10;
    std::deque<MarketDataEvent> last10;

    size_t total = 0;
    NanoTime firstTs = 0;
    NanoTime lastTs = 0;

    while (std::getline(file, line)) {
        auto evt = parseLine(line);
        if (!evt) continue;

        const auto& e = *evt;

        if (total == 0) firstTs = e.timestamp;
        lastTs = e.timestamp;

        if (first10.size() < 10)
            first10.push_back(e);

        last10.push_back(e);
        if (last10.size() > 10)
            last10.pop_front();

        processMarketDataEvent(e);

        total++;
    }

    std::cout << "\n===== SUMMARY =====\n";
    std::cout << "Total: " << total << "\n";
    std::cout << "First ts: " << firstTs << "\n";
    std::cout << "Last ts: " << lastTs << "\n";

    std::cout << "\n===== FIRST 10 =====\n";
    for (const auto& e : first10)
        std::cout << e.timestamp << " " << e.orderId << "\n";

    std::cout << "\n===== LAST 10 =====\n";
    for (const auto& e : last10)
        std::cout << e.timestamp << " " << e.orderId << "\n";

    return 0;
}