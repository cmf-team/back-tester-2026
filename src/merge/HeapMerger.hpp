// K-way merge via std::priority_queue. O(log K) per pop. One front event per
// source; after each pop, pulls one more event from the popped source.
// Prefetching / buffering is not the merger's concern — wrap a source in
// PrefetchBuffer if you want it.

#pragma once

#include "common/BasicTypes.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstddef>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace cmf {

template <class Source>
class HeapMerger {
public:
  explicit HeapMerger(std::vector<Source*> sources)
      : sources_(std::move(sources)) {
    fronts_.resize(sources_.size());
  }

  bool next(MarketDataEvent& out) {
    if (!primed_) {
      for (std::size_t i = 0; i < sources_.size(); ++i) {
        if (sources_[i]->next(fronts_[i]))
          heap_.push({fronts_[i].ts_recv, i});
      }
      primed_ = true;
    }
    if (heap_.empty()) return false;
    const Node top = heap_.top();
    heap_.pop();
    out = std::move(fronts_[top.idx]);
    if (sources_[top.idx]->next(fronts_[top.idx]))
      heap_.push({fronts_[top.idx].ts_recv, top.idx});
    return true;
  }

private:
  struct Node {
    NanoTime    ts;
    std::size_t idx;
    bool operator>(const Node& other) const noexcept { return ts > other.ts; }
  };

  std::vector<Source*>                                         sources_;
  std::vector<MarketDataEvent>                                 fronts_;
  std::priority_queue<Node, std::vector<Node>, std::greater<>> heap_;
  bool                                                         primed_{false};
};

} // namespace cmf
