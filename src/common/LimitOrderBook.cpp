#include "LimitOrderBook.h"

#include <iostream>


LimitOrderBook::LimitOrderBook() {
    bids = {};
    asks = {};
}

void LimitOrderBook::applyEvent(const MarketDataEvent &event) {
    switch (event.action[0]) {
        case 'A':
            if (event.side == "B") {
                auto level = bids.find(event.price);
                if (level != bids.end()) {
                    bids.insert({event.price, event.size});
                } else {
                    bids[event.price] = bids[event.price] + event.size;
                }
            } else {
                auto level = asks.find(event.price);
                if (level != asks.end()) {
                    asks.insert({event.price, event.size});
                } else {
                    asks[event.price] = asks[event.price] + event.size;
                }
            }
            break;
        case 'C':
            if (event.side == "B") {
                auto level = bids.find(event.price);
                if (level != bids.end()) {
                    bids[event.price] = bids[event.price] - event.size;
                    if (bids[event.price] == 0.0) {
                        bids.erase(event.price);
                    }
                } else {
                    std::cout << "smth wrong :(" << std::endl;
                }
            } else {
                auto level = asks.find(event.price);
                if (level != asks.end()) {
                    asks[event.price] = asks[event.price] - event.size;
                    if (asks[event.price] == 0.0) {
                        asks.erase(event.price);
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
