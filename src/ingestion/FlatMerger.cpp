#include "ingestion/FlatMerger.hpp"
#include <queue>
#include <utility>

namespace cmf {

FlatMerger::FlatMerger(std::vector<EventQueue*> inputs)
    : inputs_(std::move(inputs)) {}

void FlatMerger::run() {
    using Slot = std::pair<MarketDataEvent, std::size_t>;
    auto cmp   = [](const Slot& a, const Slot& b) { return a.first.ts_recv > b.first.ts_recv; };
    std::priority_queue<Slot, std::vector<Slot>, decltype(cmp)> pq(cmp);

    for (std::size_t i = 0; i < inputs_.size(); i++) {
        MarketDataEvent e = inputs_[i]->pop();
        if (e.ts_recv != MarketDataEvent::SENTINEL)
            pq.emplace(e, i);
    }

    while (!pq.empty()) {
        auto [event, idx] = pq.top();
        pq.pop();
        output_.push(event);
        MarketDataEvent next = inputs_[idx]->pop();
        if (next.ts_recv != MarketDataEvent::SENTINEL)
            pq.emplace(next, idx);
    }

    MarketDataEvent sentinel{};
    sentinel.ts_recv = MarketDataEvent::SENTINEL;
    output_.push(sentinel);
}

} // namespace cmf
