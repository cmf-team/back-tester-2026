#pragma once
#include <vector>
#include <string>
#include "models/MarketDataEvent.h"

void loadMarketData(
    const std::string& filepath, const std::function<void(const MarketDataEvent&, MarketDataEventSumary&)>& consumer
    );