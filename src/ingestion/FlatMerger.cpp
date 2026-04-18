#include "ingestion/FlatMerger.hpp"

namespace cmf {

FlatMerger::FlatMerger(std::vector<EventQueue*> inputs)
    : inputs_(std::move(inputs)) {
    heads_.resize(inputs_.size());
    keys_.assign(inputs_.size(), MarketDataEvent::SENTINEL);
}

void FlatMerger::start() {
    for (std::size_t i = 0; i < inputs_.size(); ++i) {
        inputs_[i]->pop(heads_[i]);
        keys_[i] = heads_[i].ts_recv;
        if (keys_[i] != MarketDataEvent::SENTINEL) ++live_;
    }
}

bool FlatMerger::next(MarketDataEvent& out) {
    if (live_ == 0) return false;

    const std::size_t n = keys_.size();
    const NanoTime*   k = keys_.data();
    std::size_t       min_idx = 0;
    NanoTime          min_ts  = k[0];
    for (std::size_t i = 1; i < n; ++i) {
        NanoTime t = k[i];
        if (t < min_ts) { min_ts = t; min_idx = i; }
    }

    out = heads_[min_idx];
    inputs_[min_idx]->pop(heads_[min_idx]);
    keys_[min_idx] = heads_[min_idx].ts_recv;
    if (heads_[min_idx].ts_recv == MarketDataEvent::SENTINEL) --live_;
    return true;
}

} // namespace cmf
