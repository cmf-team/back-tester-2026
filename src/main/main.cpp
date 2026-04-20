#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include "data_layer/Producer.hpp"
#include <iostream>
#include <deque>

using namespace cmf;

int main() {
    SpscQueue<MarketDataEvent> q;
    Producer prod("test_data/sample.mbo.json", q);

    std::deque<MarketDataEvent> first, last;
    constexpr int N = 5;

    prod.start();

    while (true) {
        MarketDataEvent e = q.pop();
        if (e.ts_recv == MarketDataEvent::SENTINEL) break;

        if (first.size() < N) first.push_back(e);
        last.push_back(e);
        if (last.size() > N) last.pop_front();
    }

    prod.join();

    std::cout << "=== First " << N << " events ===\n";
    for (const auto& e : first) {
        std::cout << "ts=" << e.ts_recv
                  << " sym=" << e.symbol
                  << " price=" << e.price << "\n";
    }

    std::cout << "\n=== Last " << N << " events ===\n";
    for (const auto& e : last) {
        std::cout << "ts=" << e.ts_recv
                  << " sym=" << e.symbol
                  << " price=" << e.price << "\n";
    }

    std::cout << "\nTotal: " << (first.size() >= N ? first.size() : last.size()) << " events\n";
    return 0;
}