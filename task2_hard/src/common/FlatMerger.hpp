#pragma once
#include <functional>
#include <queue>
#include <vector>
#include "MarketDataEvent.hpp"

// ── Flat Merger (single-level k-way merge) ─────────────────────────────────
// Holds the next pending event from each input stream in one min-heap.
// O(log k) per event where k = number of streams (~20 files).
// This is the classic, efficient approach for a moderate number of streams.

struct HeapEntry {
    MarketDataEvent event;
    std::size_t     stream_idx;
    bool operator>(const HeapEntry& o) const { return event.ts > o.event.ts; }
};

class FlatMerger {
public:
    using Stream = std::vector<MarketDataEvent>;

    explicit FlatMerger(std::vector<Stream> streams)
        : streams_(std::move(streams)), indices_(streams_.size(), 0)
    {
        for (std::size_t i = 0; i < streams_.size(); ++i) push_next(i);
    }

    // Returns false when all streams are exhausted
    bool next(MarketDataEvent& out) {
        if (heap_.empty()) return false;
        auto top = heap_.top(); heap_.pop();
        out = top.event;
        push_next(top.stream_idx);
        return true;
    }

    bool        empty()        const { return heap_.empty(); }
    std::size_t stream_count() const { return streams_.size(); }

private:
    void push_next(std::size_t i) {
        if (indices_[i] < streams_[i].size())
            heap_.push({streams_[i][indices_[i]++], i});
    }

    std::vector<Stream>    streams_;
    std::vector<std::size_t> indices_;
    std::priority_queue<HeapEntry,
                        std::vector<HeapEntry>,
                        std::greater<HeapEntry>> heap_;
};
