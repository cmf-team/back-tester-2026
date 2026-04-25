#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.hpp"
#include "LobRouter.hpp"
#include "MarketDataEvent.hpp"
#include "PerfStats.hpp"

using json = nlohmann::json;

int RunDataIngestionFile(const std::string& filePath)
{
    PerfStats stats;
    stats.start();

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Cannot open file: " << filePath << std::endl;
        return 2;
    }

    std::vector<MarketDataEvent> events;
    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        try
        {
            json j = json::parse(line);
            events.push_back(MarketDataEvent::fromJson(j));
        }
        catch (const std::exception& ex)
        {
            std::cerr << "JSON parse error: " << ex.what() << std::endl;
        }
    }

    std::stable_sort(events.begin(), events.end(),
        [](const MarketDataEvent& a, const MarketDataEvent& b)
        {
            if (a.tsRecv != b.tsRecv)
                return a.tsRecv < b.tsRecv;

            if (a.tsEvent != b.tsEvent)
                return a.tsEvent < b.tsEvent;

            return a.sequence < b.sequence;
        });

    LobRouter router(50000);

    for (const auto& e : events)
    {
        router.route(e);
    }

    router.printFinalState(std::cout);
    router.printStats(std::cout);

    stats.finish(events.size());
    stats.print(std::cout);

    return 0;
}