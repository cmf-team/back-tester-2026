#include "Summary.h"
#include "MarketDataEvent.h"
#include <iostream>

void Summary::update(const MarketDataEvent& e) {
    if (total == 0) {
        first_ts = e.ts_recv;
    }

    last_ts = e.ts_recv;
    total++;
}

void Summary::print() const {
    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "Total messages: " << total << "\n";
    std::cout << "First timestamp: " << first_ts << "\n";
    std::cout << "Last timestamp: " << last_ts << "\n";
}

int Summary::getTotal() const {
    return total;
}

std::string Summary::getFirstTs() const {
    return first_ts;
}

std::string Summary::getLastTs() const {
    return last_ts;
}