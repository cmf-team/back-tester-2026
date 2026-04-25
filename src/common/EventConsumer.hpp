#pragma once

#include "common/MarketDataEvent.hpp"

#include <iosfwd>

namespace cmf {
    void printEvent(std::ostream &os, const MarketDataEvent &ev);


    void processMarketDataEvent(const MarketDataEvent &ev);
} // namespace cmf
