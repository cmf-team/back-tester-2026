#include "MarketDataProcessor.h"
#include <iostream>

void processMarketDataEvent(const MarketDataEvent& event, MarketDataEventSumary& summary) {
    summary.last_timestamp = event.ts_event;
    summary.total_message_count++;
    summary.lastEvents.push_back(event);
    if (summary.lastEvents.size() > 10) {
        summary.lastEvents.pop_front();
    }
    if (summary.total_message_count == 1) {
        summary.first_timestamp = event.ts_event;
    }
    if (summary.total_message_count <= 10) {
        summary.firstEvents.push_back(event);
    }

    std::cout << "Symbol: " << event.symbol
              << " | Price: " << event.price
              << " | Size: "  << event.size
              << " | Side: "  << event.side
              << " | Action: "<< event.action
              << std::endl;
}