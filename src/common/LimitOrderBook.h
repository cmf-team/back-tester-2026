#pragma once
#include <iostream>
#include <map>
#include <unordered_map>

#include "../models/MarketDataEvent.h"

constexpr int64_t PRICE_SCALE = 100000000LL; // 1e8

struct Level {
    double price;
    double size;

    friend std::ostream& operator<<(std::ostream& os, const Level& level) {
        os << "price=" << level.price << ", size=" << level.size;
        return os;
    }
};

struct BBO {
    Level bestBid;
    Level bestAsk;

    friend std::ostream& operator<<(std::ostream& os, const BBO& bbo) {
        os << "bid: " << bbo.bestBid << "; ask: " << bbo.bestAsk;
        return os;
    }
};

struct OrderBookSnapshot {
    std::vector<Level> bids;
    std::vector<Level> asks;

    friend std::ostream& operator<<(std::ostream& os, const OrderBookSnapshot& snapshot) {
        int i = 1;
        os << "bids: " << std::endl;
        for (const auto& level : snapshot.bids) {
            os << i << ": " << level << "\n";
            ++i;
        }
        i = 1;
        os << "bids: " << std::endl;
        for (const auto& level : snapshot.asks) {
            os << i << ": " << level << "\n";
            ++i;
        }
        return os;
    }
};

class LimitOrderBook {
    std::map<int64_t, double, std::greater<>> bids;
    std::map<int64_t, double> asks;

public:
    LimitOrderBook();
    void applyEvent(const MarketDataEvent &event);

    double volumeAtPrice(double price);
    BBO bestBidAsk();
    OrderBookSnapshot orderBookSnapshot();
};
