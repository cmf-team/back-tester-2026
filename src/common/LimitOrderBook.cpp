#include "LimitOrderBook.h"

#include <cmath>
#include <iostream>


LimitOrderBook::LimitOrderBook() {
    bids = {};
    asks = {};
}

int64_t toTick(double price) {
    return static_cast<int64_t>(std::round(price * PRICE_SCALE));
}

double toDoublefromTick(int64_t tick) {
    return static_cast<double>(tick) / PRICE_SCALE;
}

void LimitOrderBook::applyEvent(const MarketDataEvent &event) {
    int64_t tickPrice = toTick(event.price);
    switch (event.action[0]) {
        case 'A':
            if (event.side == "B") {
                auto level = bids.find(tickPrice);
                if (level != bids.end()) {
                    bids.insert({tickPrice, event.size});
                } else {
                    bids[tickPrice] = bids[tickPrice] + event.size;
                }
            } else {
                auto level = asks.find(tickPrice);
                if (level != asks.end()) {
                    asks.insert({tickPrice, event.size});
                } else {
                    asks[tickPrice] = asks[tickPrice] + event.size;
                }
            }
            break;
        case 'C':
            if (event.side == "B") {
                auto level = bids.find(tickPrice);
                if (level != bids.end()) {
                    bids[tickPrice] = bids[tickPrice] - event.size;
                    if (bids[tickPrice] == 0.0) {
                        bids.erase(tickPrice);
                    }
                } else {
                    std::cout << "smth wrong :(" << std::endl;
                }
            } else {
                auto level = asks.find(tickPrice);
                if (level != asks.end()) {
                    asks[tickPrice] = asks[tickPrice] - event.size;
                    if (asks[tickPrice] == 0.0) {
                        asks.erase(tickPrice);
                    }
                } else {
                    std::cout << "smth wrong :(" << std::endl;
                }
            }
            break;
        case 'R':
            bids = {};
            asks = {};
            break;
        case 'M':
        case 'T':
        case 'F':
        case 'N':
        default:
            break;
    }
}


double LimitOrderBook::volumeAtPrice(double price) {
    auto bidLevel = bids.find(toTick(price));
    if (bidLevel != bids.end()) {
        return bids[price];
    }
    auto askLevel = asks.find(toTick(price));
    if (askLevel != asks.end()) {
        return asks[price];
    }
    return 0.0;
}

BBO LimitOrderBook::bestBidAsk() {
    double bestBidPrice = bids.begin()->first;
    double bestBidSize = bids[bestBidPrice];
    double bestAskPrice = asks.begin()->first;
    double bestAskSize = asks[bestAskPrice];
    return BBO{
        Level{toDoublefromTick(bestBidPrice), bestBidSize},
        Level{toDoublefromTick(bestAskPrice), bestAskSize}
    };
}

OrderBookSnapshot LimitOrderBook::orderBookSnapshot() {
    OrderBookSnapshot snapshot;
    for (auto it = bids.begin(); it != bids.end(); ++it) {
        snapshot.bids.push_back(Level{toDoublefromTick(it->first), it->second});
    }
    for (auto it = asks.begin(); it != asks.end(); ++it) {
        snapshot.asks.push_back(Level{toDoublefromTick(it->first), it->second});
    }
    return snapshot;
}
