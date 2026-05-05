// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"
#include "common/MarketDataEvent.hpp"
#include "main/FileReader.hpp"

#include <cstddef>
#include <deque>
#include <iostream>
#include <string>
#include <vector>

using namespace cmf;

namespace
{

void processMarketDataEvent(const MarketDataEvent &order)
{
    std::cout << "ts_recv=" << order.tsRecv << " order_id=" << order.orderId
              << " side=" << MarketDataEvent::toChar(order.side) << " price=";
    if (order.price.has_value())
    {
        std::cout << *order.price;
    }
    else
    {
        std::cout << "null";
    }
    std::cout << " size=" << order.size << " action=" << MarketDataEvent::toChar(order.action) << std::endl;
}

} // namespace

int main(int argc, const char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: back-tester <input_file_path>" << std::endl;
            return 1;
        }

        const std::string inputFilePath{argv[1]};
        FileReader fileReader(inputFilePath);
        MarketDataEvent event;
        std::size_t totalMessagesProcessed = 0;
        std::string firstTimestamp;
        std::string lastTimestamp;
        std::vector<MarketDataEvent> firstTenEvents;
        std::deque<MarketDataEvent> lastTenEvents;

        while (fileReader.readNextEvent(event))
        {
            ++totalMessagesProcessed;

            if (firstTimestamp.empty())
            {
                firstTimestamp = event.tsRecv;
            }
            lastTimestamp = event.tsRecv;

            if (firstTenEvents.size() < 10)
            {
                firstTenEvents.push_back(event);
            }

            lastTenEvents.push_back(event);
            if (lastTenEvents.size() > 10)
            {
                lastTenEvents.pop_front();
            }
        }

        std::cout << "First 10 events:" << std::endl;
        for (const auto &firstEvent : firstTenEvents)
        {
            processMarketDataEvent(firstEvent);
        }

        std::cout << "Last 10 events:" << std::endl;
        for (const auto &lastEvent : lastTenEvents)
        {
            processMarketDataEvent(lastEvent);
        }

        std::cout << "Summary: total_messages_processed=" << totalMessagesProcessed
                  << " first_timestamp=" << (firstTimestamp.empty() ? "n/a" : firstTimestamp)
                  << " last_timestamp=" << (lastTimestamp.empty() ? "n/a" : lastTimestamp) << std::endl;
    }
    catch (std::exception &ex)
    {
        std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
