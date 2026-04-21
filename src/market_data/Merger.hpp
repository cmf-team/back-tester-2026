// Merger — merges N chronologically-sorted IEventSource streams into one.
//
// Two strategies are provided (task_1.md §Hard Task):
//   * FlatMerger      — single k-way min-heap across all source heads.
//   * HierarchyMerger — binary tree of PairMergers.
//
// Both preserve strict chronological order by (ts_recv, sequence,
// instrument_id) (see MarketDataEvent::operator<).

#pragma once

#include "market_data/EventSource.hpp"
#include "market_data/MarketDataEvent.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace cmf {

class IMerger {
public:
  IMerger() = default;
  virtual ~IMerger() = default;

  IMerger(const IMerger &) = delete;
  IMerger &operator=(const IMerger &) = delete;

  // Returns the globally-next event across all sources, or false when all
  // sources are exhausted.
  virtual bool next(MarketDataEvent &out) = 0;
};

// ---------------------------------------------------------------------------
// FlatMerger: one heap holding at most one pending event per source.
// ---------------------------------------------------------------------------
class FlatMerger final : public IMerger {
public:
  explicit FlatMerger(std::vector<IEventSource *> sources);

  bool next(MarketDataEvent &out) override;

private:
  struct HeapEntry {
    MarketDataEvent event;
    std::size_t src_idx;
  };
  struct Greater {
    bool operator()(const HeapEntry &a, const HeapEntry &b) const noexcept {
      return a.event > b.event;
    }
  };

  std::vector<IEventSource *> sources_;
  std::vector<HeapEntry> heap_; // min-heap via std::greater over events
};

// ---------------------------------------------------------------------------
// HierarchyMerger: binary tree of pairwise mergers.
// ---------------------------------------------------------------------------
class HierarchyMerger final : public IMerger {
public:
  // Opaque internal tree node type; defined in Merger.cpp. Declared public
  // only so LeafNode/PairNode subclasses (in Merger.cpp's anonymous
  // namespace) can derive from it. Not part of the stable API.
  class Node;

  explicit HierarchyMerger(const std::vector<IEventSource *> &sources);
  ~HierarchyMerger() override;

  bool next(MarketDataEvent &out) override;

private:
  std::unique_ptr<Node> root_;
};

} // namespace cmf
