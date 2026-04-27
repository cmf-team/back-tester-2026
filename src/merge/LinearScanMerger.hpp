// K-way merge via linear scan of source fronts. O(K) per pop, no prefetch —
// each pop calls Source::next() exactly once on the popped source. Serves as
// the baseline in benchmarks against prefetch-enabled variants.

#pragma once

#include "common/BasicTypes.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace cmf {

template <class Source>
class LinearScanMerger {
public:
  explicit LinearScanMerger(std::vector<Source*> sources) {
    slots_.reserve(sources.size());
    for (auto* s : sources) {
      slots_.emplace_back();
      slots_.back().src = s;
    }
  }

  bool next(MarketDataEvent& out) {
    if (!primed_) {
      for (auto& sl : slots_)
        if (!sl.src->next(sl.ev)) sl.exhausted = true;
      primed_ = true;
    }
    constexpr NanoTime kInf = (std::numeric_limits<NanoTime>::max)();
    NanoTime    best_ts = kInf;
    std::size_t best    = slots_.size();
    for (std::size_t i = 0; i < slots_.size(); ++i) {
      if (slots_[i].exhausted) continue;
      const NanoTime ts = slots_[i].ev.ts_recv;
      if (ts < best_ts) { best_ts = ts; best = i; }
    }
    if (best == slots_.size()) return false;
    out = std::move(slots_[best].ev);
    if (!slots_[best].src->next(slots_[best].ev))
      slots_[best].exhausted = true;
    return true;
  }

private:
  struct Slot {
    Source*         src{nullptr};
    MarketDataEvent ev{};
    bool            exhausted{false};
  };

  std::vector<Slot> slots_;
  bool              primed_{false};
};

} // namespace cmf
