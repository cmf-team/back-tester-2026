// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"
#include "common/MarketDataEvent.hpp"
#include "main/FileReader.hpp"
#include "main/MdEventConverter.hpp"

#include <cstddef>
#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace cmf;

namespace
{

void processMarketDataEvent(const MarketDataEvent &order)
{
    std::cout << "ts_recv_ns=" << std::to_string(order.tsRecv) << " order_id=" << order.orderId
              << " side=" << MdEventConverter::toChar(order.side) << " price=";
    if (order.price.has_value())
    {
        std::cout << *order.price;
    }
    else
    {
        std::cout << "null";
    }
    std::cout << " size=" << order.size << " action=" << MdEventConverter::toChar(order.action) << std::endl;
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
        MdEventConverter eventConverter;
        MarketDataEvent event;
        std::string rawLine;
        std::size_t totalMessagesProcessed = 0;
        std::int64_t earliestTimestampNs = std::numeric_limits<std::int64_t>::max();
        std::int64_t latestTimestampNs = std::numeric_limits<std::int64_t>::min();
        std::vector<MarketDataEvent> firstTenEvents;
        std::deque<MarketDataEvent> lastTenEvents;

        while (fileReader.readNextRawLine(rawLine))
        {
            if (!eventConverter.parseRaw(rawLine, event))
            {
                continue;
            }
            ++totalMessagesProcessed;

            if (event.tsRecv < earliestTimestampNs)
            {
                earliestTimestampNs = event.tsRecv;
            }
            if (event.tsRecv > latestTimestampNs)
            {
                latestTimestampNs = event.tsRecv;
            }

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

        std::cout << "Summary: total_messages_processed=" << totalMessagesProcessed << " first_timestamp_ns=";
        if (totalMessagesProcessed == 0)
        {
            std::cout << "n/a first_timestamp=n/a last_timestamp_ns=n/a last_timestamp=n/a";
        }
        else
        {
            char earliestIso[31];
            char latestIso[31];
            MdEventConverter::nanosToIsoTimestamp(earliestTimestampNs, earliestIso);
            MdEventConverter::nanosToIsoTimestamp(latestTimestampNs, latestIso);
            std::cout << earliestTimestampNs
                      << " first_timestamp=" << earliestIso
                      << " last_timestamp_ns=" << latestTimestampNs
                      << " last_timestamp=" << latestIso;
        }
        std::cout << std::endl;
    }
    catch (std::exception &ex)
    {
        std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
