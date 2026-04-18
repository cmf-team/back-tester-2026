#pragma once

#include "ingestion/EventQueue.hpp"
#include <cstddef>
#include <vector>

namespace cmf {

class FlatMerger {
public:
    explicit FlatMerger(std::vector<EventQueue*> inputs);

    void start();
    bool next(MarketDataEvent& out);

private:
    std::vector<EventQueue*>     inputs_;
    std::vector<MarketDataEvent> heads_;
    std::vector<NanoTime>        keys_;
    std::size_t                  live_ = 0;
};

} // namespace cmf
