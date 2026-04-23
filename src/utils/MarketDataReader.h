#pragma once
#include <vector>
#include <string>

#include "common/LimitOrderBook.h"
#include "models/MarketDataEvent.h"

void loadMarketData(
    const std::string& filepath, const std::function<void(const MarketDataEvent&, std::map<std::string, LimitOrderBook> &)>& consumer
    );