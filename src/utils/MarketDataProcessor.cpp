#include "MarketDataProcessor.h"
#include <iostream>

void processMarketDataEvent(const MarketDataEvent& event, std::map<std::string, LimitOrderBook> & orderbooks) {
    std::string key = std::to_string(event.instrument_id);
    orderbooks[key].applyEvent(event);

    if (rand() % 20000 == 0) {
        auto orderbook = orderbooks[key];
        std::cout << "Instrument: " << key << std::endl
                  << "BBO: " << orderbook.bestBidAsk() << std::endl
                  << "Snapshot: " << orderbook.orderBookSnapshot() << std::endl;
    }
}