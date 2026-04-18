#include <iostream>
#include "Dispatcher.hpp"

namespace cmf {

void processMarketDataEvent(const MarketDataEvent& event) {
    std::cout << "timestamp:" << event.tsRecvStr << "\n";
    std::cout << "orderId:"   << event.orderIdStr << "\n";
    std::cout << "side:"      << event.side << "\n";
    std::cout << "price:"     << event.priceStr << "\n";
    std::cout << "size:"      << event.size << "\n";
    std::cout << "action:"    << event.action << "\n";
    std::cout << "----\n";
}

}