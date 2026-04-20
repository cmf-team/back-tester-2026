#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include "data_layer/EventHierarchyMerger.hpp"
#include <iostream>
#include <vector>
#include <cassert>

using namespace cmf;

int main() {
    // Создаём 4 очереди с перемешанными временными метками
    SpscQueue<MarketDataEvent> q1, q2, q3, q4;

    // Q1: 100, 500, 900
    for (auto ts : {100, 500, 900}) {
        MarketDataEvent e{}; e.ts_recv = ts * 1000; q1.push(e);
    }
    // Q2: 200, 600, 1000
    for (auto ts : {200, 600, 1000}) {
        MarketDataEvent e{}; e.ts_recv = ts * 1000; q2.push(e);
    }
    // Q3: 300, 700, 1100
    for (auto ts : {300, 700, 1100}) {
        MarketDataEvent e{}; e.ts_recv = ts * 1000; q3.push(e);
    }
    // Q4: 400, 800, 1200
    for (auto ts : {400, 800, 1200}) {
        MarketDataEvent e{}; e.ts_recv = ts * 1000; q4.push(e);
    }

    // SENTINEL в каждую
    for (auto* q : {&q1, &q2, &q3, &q4}) {
        MarketDataEvent s{}; s.ts_recv = MarketDataEvent::SENTINEL;
        q->push(s);
    }

    // Запускаем мерджер
    std::vector<EventQueue*> inputs = {&q1, &q2, &q3, &q4};
    HierarchyMerger merger(inputs);
    merger.start();

    // Считываем и проверяем порядок
    std::vector<NanoTime> result;
    MarketDataEvent e;
    while (merger.next(e)) {
        result.push_back(e.ts_recv);
    }
    merger.join();

    // Ожидаемый порядок: 100..1200 с шагом 100
    for (std::size_t i = 0; i < result.size(); ++i) {
        assert(result[i] == (100 + i * 100) * 1000);
    }

    std::cout << "HierarchyMerger test passed! Global order preserved.\n";
    std::cout << "Output: ";
    for (auto ts : result) std::cout << ts/1000 << " ";
    std::cout << "\n";

    return 0;
}