#pragma once

#include "domain/MarketDataEvent.hpp"

namespace md {

class IMarketDataEventProcessor {
public:
    virtual ~IMarketDataEventProcessor() = default;
    virtual void processMarketDataEvent(const MarketDataEvent& event) = 0;
};

} // namespace md
