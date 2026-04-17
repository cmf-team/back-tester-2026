#include "ingestion/FlatMerger.hpp"
#include <queue>
#include <utility>

namespace cmf {

FlatMerger::FlatMerger(std::vector<EventQueue*> inputs, std::size_t out_cap)
    : inputs_(std::move(inputs)), output_(out_cap) {}

void FlatMerger::run() {
    using Slot = std::pair<MarketDataEvent, std::size_t>;
    auto cmp   = [](const Slot& a, const Slot& b) {
        return a.first.ts_recv > b.first.ts_recv;
    };
    std::priority_queue<Slot, std::vector<Slot>, decltype(cmp)> pq(cmp);

    for (std::size_t i = 0; i < inputs_.size(); i++) {
        if (auto opt = inputs_[i]->pop(); opt)
            pq.emplace(*opt, i);
    }

    while (!pq.empty()) {
        auto [event, idx] = pq.top();
        pq.pop();
        output_.push(std::make_optional(std::move(event)));
        if (auto opt = inputs_[idx]->pop(); opt)
            pq.emplace(*opt, idx);
    }

    output_.push(std::nullopt);
}

} // namespace cmf
