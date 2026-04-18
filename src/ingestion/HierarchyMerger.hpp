#pragma once

#include "ingestion/EventQueue.hpp"
#include <memory>
#include <thread>
#include <vector>

namespace cmf {

class HierarchyMerger {
public:
    explicit HierarchyMerger(std::vector<EventQueue*> leaf_inputs);
    ~HierarchyMerger();

    void start();
    bool next(MarketDataEvent& out);
    void join();

private:
    struct MergeSpec {
        EventQueue* left;
        EventQueue* right;
        EventQueue* out;
    };

    static void merge_two(EventQueue& left, EventQueue& right, EventQueue& out);

    std::vector<std::unique_ptr<EventQueue>> node_queues_;
    std::vector<MergeSpec>                   specs_;
    std::vector<std::thread>                 threads_;

    EventQueue*     root_left_  = nullptr;
    EventQueue*     root_right_ = nullptr;
    EventQueue*     single_     = nullptr;
    MarketDataEvent buf_left_{};
    MarketDataEvent buf_right_{};
    bool            primed_     = false;
};

} // namespace cmf
