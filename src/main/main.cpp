#include "common/SpscQueue.hpp"
#include "common/MarketDataEvent.hpp"
#include <thread>
#include <iostream>

using namespace cmf;

int main() {
    SpscQueue<MarketDataEvent, 1024> q;
    std::size_t count = 0;

    // Consumer
    std::thread consumer([&] {
        MarketDataEvent e;
        while (true) {
            q.pop(e);  // блокирует
            if (e.ts_recv == MarketDataEvent::SENTINEL) break;
            count++;
        }
        std::cout << "Received: " << count << " events\n";
    });

    // Producer
    for (std::size_t i = 0; i < 100; ++i) {
        MarketDataEvent e{};
        e.ts_recv = i * 1000;
        q.push(e);
    }
    // Sentinel — сигнал конца
    MarketDataEvent sentinel{};
    sentinel.ts_recv = MarketDataEvent::SENTINEL;
    q.push(sentinel);

    consumer.join();
    return 0;
}