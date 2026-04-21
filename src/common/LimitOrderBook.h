#pragma once
#include <unordered_map>

#include "../models/MarketDataEvent.h"

class LimitOrderBook {
private:
    std::unordered_map<double, double> bids;
    std::unordered_map<double, double> asks;

public:
    LimitOrderBook();
    void applyEvent(const MarketDataEvent &event);
};
