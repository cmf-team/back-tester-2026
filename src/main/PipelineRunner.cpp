#include "main/PipelineRunner.hpp"

#include "main/FileReader.hpp"
#include "main/MdEventConverter.hpp"

#include <deque>
#include <limits>
#include <string>

namespace cmf
{

PipelineRunner::PipelineResult PipelineRunner::run(const std::string &inputFilePath) const
{
    FileReader fileReader(inputFilePath);
    MdEventConverter eventConverter;
    PipelineResult result;
    MarketDataEvent event;
    std::string rawLine;
    NanoTime earliestTimestampNs = std::numeric_limits<NanoTime>::max();
    NanoTime latestTimestampNs = std::numeric_limits<NanoTime>::min();
    std::deque<MarketDataEvent> lastTenEvents;

    while (fileReader.readNextRawLine(rawLine))
    {
        if (!eventConverter.parseRaw(rawLine, event))
        {
            continue;
        }
        ++result.totalMessagesProcessed;

        if (event.tsRecv < earliestTimestampNs)
        {
            earliestTimestampNs = event.tsRecv;
        }
        if (event.tsRecv > latestTimestampNs)
        {
            latestTimestampNs = event.tsRecv;
        }

        if (result.firstTenEvents.size() < 10)
        {
            result.firstTenEvents.push_back(event);
        }
        lastTenEvents.push_back(event);
        if (lastTenEvents.size() > 10)
        {
            lastTenEvents.pop_front();
        }
    }

    result.lastTenEvents.assign(lastTenEvents.begin(), lastTenEvents.end());
    if (result.totalMessagesProcessed > 0)
    {
        result.earliestTimestampNs = earliestTimestampNs;
        result.latestTimestampNs = latestTimestampNs;
    }
    return result;
}

} // namespace cmf
