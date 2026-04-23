#pragma once
#include "common/LimitOrderBook.h"
#include "models/MarketDataEvent.h"

void processMarketDataEvent(const MarketDataEvent& event, std::map<std::string, LimitOrderBook> & orderbooks);