#pragma once

#include "ingestion/EventQueue.hpp"
#include <memory>
#include <thread>
#include <vector>

namespace cmf {

// Multi-level binary merge tree.
// Pairs up inputs at each level, spawning one thread per merge node.
// output() returns the root queue consumed by the dispatcher.
class HierarchyMerger {
public:
    HierarchyMerger(std::vector<EventQueue*> leaf_inputs, std::size_t node_cap);
    ~HierarchyMerger();

    void        start();
    void        join();
    EventQueue& output() { return *final_queue_; }

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
    EventQueue*                              final_queue_ = nullptr;
};

} // namespace cmf
