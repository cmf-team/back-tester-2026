// processMarketDataEvent: free function with the exact signature mandated
// by HW-1 ("void processMarketDataEvent(const MarketDataEvent& order)").
// Forwards every event to the process-wide EventCollector. Once the LOB
// engine arrives, this function will route into the LOB instead.

#pragma once

#include "parser/MarketDataEvent.hpp"

namespace cmf {

void processMarketDataEvent(const MarketDataEvent &order);

} // namespace cmf
