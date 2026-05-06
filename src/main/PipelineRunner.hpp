#pragma once

#include "common/BasicTypes.hpp"
#include "common/MarketDataEvent.hpp"
#include <optional>
#include <string>
#include <vector>

namespace cmf
{

class PipelineRunner
{
  public:
    struct PipelineResult
    {
        std::size_t totalMessagesProcessed = 0;
        std::optional<NanoTime> earliestTimestampNs;
        std::optional<NanoTime> latestTimestampNs;
        std::vector<MarketDataEvent> firstTenEvents;
        std::vector<MarketDataEvent> lastTenEvents;
    };

    PipelineResult run(const std::string &inputFilePath) const;
};

} // namespace cmf
