// MergerFlat: classic k-way merge built on a single std::priority_queue.
//
// Each cell on the heap is (timestamp, source_index). We pop the smallest
// timestamp, emit its event, then refill that source. Complexity per
// emitted event is O(log k) where k = number of input sources.
//
// We deliberately store the heap entries with the timestamp duplicated in
// the entry rather than peeking at the cached event each time -- this
// keeps the std::priority_queue comparator a pure function of the entry
// itself and avoids reaching into mutable state.

#pragma once

#include "pipeline/IEventSource.hpp"

#include <cstddef>
#include <cstdint>
#include <queue>
#include <vector>

namespace cmf {

class MergerFlat final : public IEventSource {
public:
  explicit MergerFlat(std::vector<EventSourcePtr> sources);

  bool next(MarketDataEvent &out) override;

  // ---- diagnostics -------------------------------------------------------
  std::size_t   sources() const noexcept { return sources_.size(); }
  std::uint64_t emitted() const noexcept { return emitted_; }

private:
  // One slot per input source: holds the most recently fetched event so we
  // can hand it back when the heap pops its index.
  std::vector<EventSourcePtr>      sources_;
  std::vector<MarketDataEvent>     buffered_;
  std::vector<bool>                exhausted_;

  struct HeapEntry {
    NanoTime     ts;
    std::size_t  idx;
    // min-heap: larger timestamp -> lower priority
    bool operator<(const HeapEntry &o) const noexcept { return ts > o.ts; }
  };
  std::priority_queue<HeapEntry>   heap_;

  std::uint64_t emitted_{0};

  void refill(std::size_t idx);
};

} // namespace cmf
