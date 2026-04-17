#pragma once

#include "ingestion/EventQueue.hpp"
#include <vector>

namespace cmf {

// Single-level k-way merge of N chronologically-sorted producer queues.
// Reads one event per producer into a priority queue; always pops the minimum.
class FlatMerger {
public:
    FlatMerger(std::vector<EventQueue*> inputs, std::size_t out_cap);

    void        run();
    EventQueue& output() { return output_; }

private:
    std::vector<EventQueue*> inputs_;
    EventQueue               output_;
};

} // namespace cmf
