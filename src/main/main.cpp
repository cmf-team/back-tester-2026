#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include "data_layer/EventFlatMerger.hpp"
#include <iostream>
#include <vector>
#include <cassert>

using namespace cmf;

int main() {
    // Создаём 3 очереди с перемешанными временными метками
    SpscQueue<MarketDataEvent> q1, q2, q3;

    // Q1: ts = 100, 400, 700
    for (auto ts : {100, 400, 700}) {
        MarketDataEvent e{}; e.ts_recv = ts * 1000;
        q1.push(e);
    }
    // Q2: ts = 200, 500, 800
    for (auto ts : {200, 500, 800}) {
        MarketDataEvent e{}; e.ts_recv = ts * 1000;
        q2.push(e);
    }
    // Q3: ts = 300, 600, 900
    for (auto ts : {300, 600, 900}) {
        MarketDataEvent e{}; e.ts_recv = ts * 1000;
        q3.push(e);
    }

    // SENTINEL в каждую очередь
    for (auto* q : {&q1, &q2, &q3}) {
        MarketDataEvent s{}; s.ts_recv = MarketDataEvent::SENTINEL;
        q->push(s);
    }

    // Запускаем мерджер
    std::vector<SpscQueue<MarketDataEvent>*> inputs = {&q1, &q2, &q3};
    FlatMerger merger(inputs);
    merger.start();

    // Считываем и проверяем порядок
    std::vector<NanoTime> result;
    MarketDataEvent e;
    while (merger.next(e)) {
        result.push_back(e.ts_recv);
    }

    // Ожидаемый порядок: 100,200,300,400,500,600,700,800,900 (в тыс. наносек)
    std::vector<NanoTime> expected = {
        100000, 200000, 300000, 400000, 500000,
        600000, 700000, 800000, 900000
    };

    assert(result.size() == expected.size());
    for (std::size_t i = 0; i < result.size(); ++i) {
        assert(result[i] == expected[i]);
    }

    std::cout << "FlatMerger test passed! Global order preserved.\n";
    std::cout << "Output: ";
    for (auto ts : result) std::cout << ts/1000 << " ";
    std::cout << "\n";

    return 0;
}