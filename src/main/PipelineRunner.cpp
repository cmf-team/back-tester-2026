#include "main/PipelineRunner.hpp"

#include "common/MarketDataEvent.hpp"
#include "main/FileReader.hpp"
#include "main/MdEventConverter.hpp"

#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace cmf
{

namespace
{

void printMarketDataEvent(const MarketDataEvent &event)
{
    std::cout << "ts_recv_ns=" << event.tsRecv << " order_id=" << event.orderId
              << " side=" << MdEventConverter::toChar(event.side) << " price=";
    if (event.price.has_value())
    {
        std::cout << *event.price;
    }
    else
    {
        std::cout << "null";
    }
    std::cout << " size=" << event.size << " action=" << MdEventConverter::toChar(event.action) << std::endl;
}

} // namespace

void PipelineRunner::run(const std::string &inputFilePath) const
{
    FileReader fileReader(inputFilePath);
    MdEventConverter eventConverter;
    MarketDataEvent event;
    std::string rawLine;
    std::size_t totalMessagesProcessed = 0;
    NanoTime earliestTimestampNs = std::numeric_limits<NanoTime>::max();
    NanoTime latestTimestampNs = std::numeric_limits<NanoTime>::min();
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
        printMarketDataEvent(firstEvent);
    }

    std::cout << "Last 10 events:" << std::endl;
    for (const auto &lastEvent : lastTenEvents)
    {
        printMarketDataEvent(lastEvent);
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

} // namespace cmf
