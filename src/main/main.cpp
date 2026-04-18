#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <cstdint>

#include "../common/MarketDataEvent.hpp"
#include "../common/Parser.hpp"
#include "../common/Dispatcher.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: program <path_to_file>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        std::cerr << "Cannot open file\n";
        return 1;
    }

    std::vector<cmf::MarketDataEvent> first10;
    std::deque<cmf::MarketDataEvent> last10;
    std::string firstTimestamp;
    std::string lastTimestamp;
    std::uint32_t total = 0;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        try {
            cmf::MarketDataEvent event = cmf::parseLine(line);
            total++;

            if (total == 1)
                firstTimestamp = event.tsRecvStr;
            lastTimestamp = event.tsRecvStr;

            if (first10.size() < 10)
                first10.push_back(event);

            last10.push_back(event);
            if (last10.size() > 10)
                last10.pop_front();

        } catch (const std::exception& e) {
            std::cerr << "Parse error: " << e.what() << "\n";
        }
    }

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Total messages: " << total << "\n";
    std::cout << "First timestamp: " << firstTimestamp << "\n";
    std::cout << "Last timestamp: "  << lastTimestamp << "\n";

    std::cout << "\n=== FIRST 10 ===\n";
    for (const auto& ev : first10)
        cmf::processMarketDataEvent(ev);

    std::cout << "\n=== LAST 10 ===\n";
    for (const auto& ev : last10)
        cmf::processMarketDataEvent(ev);

    return 0;
}