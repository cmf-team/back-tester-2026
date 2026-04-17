#pragma once

#include "ingestion/EventQueue.hpp"
#include <vector>

namespace cmf {

class FlatMerger {
public:
    explicit FlatMerger(std::vector<EventQueue*> inputs);

    void        run();
    EventQueue& output() { return output_; }

private:
    std::vector<EventQueue*> inputs_;
    EventQueue               output_;
};

} // namespace cmf
