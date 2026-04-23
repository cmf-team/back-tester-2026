// K-way merge via Knuth's loser tree (TAOCP vol. 3, §5.4.1). O(log K) per pop
// with ~half the comparisons of a heap. One front event per source; after
// each pop, pulls one more event from the popped source. Prefetching is not
// the merger's concern — wrap a source in PrefetchBuffer if you want it.
//
// K is padded to a power of two; virtual sentinel sources hold +inf keys and
// never win.

#pragma once

#include "common/BasicTypes.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace cmf {

template <class Source>
class LoserTreeMerger {
public:
  explicit LoserTreeMerger(std::vector<Source*> sources)
      : sources_(std::move(sources)) {
    K_ = 1;
    while (K_ < sources_.size()) K_ *= 2;
    if (K_ < 2) K_ = 2;
    fronts_.resize(sources_.size());
    exhausted_.assign(sources_.size(), false);
    cur_key_.assign(K_, kInf);
    tree_.assign(K_, 0);
  }

  bool next(MarketDataEvent& out) {
    if (!primed_) {
      for (std::size_t i = 0; i < sources_.size(); ++i) pullOne(i);
      build();
      primed_ = true;
    }
    const std::size_t w = tree_[0];
    if (cur_key_[w] == kInf) return false;
    out = std::move(fronts_[w]);
    pullOne(w);
    adjust(w);
    return true;
  }

private:
  static constexpr NanoTime kInf = (std::numeric_limits<NanoTime>::max)();

  void pullOne(std::size_t i) {
    if (i >= sources_.size() || exhausted_[i]) {
      cur_key_[i] = kInf;
      return;
    }
    if (!sources_[i]->next(fronts_[i])) {
      exhausted_[i] = true;
      cur_key_[i]   = kInf;
    } else {
      cur_key_[i] = fronts_[i].ts_recv;
    }
  }

  void build() {
    std::vector<std::size_t> w(2 * K_);
    for (std::size_t i = 0; i < K_; ++i) w[K_ + i] = i;
    for (std::size_t pos = K_ - 1; pos >= 1; --pos) {
      const std::size_t l = w[2 * pos];
      const std::size_t r = w[2 * pos + 1];
      if (cur_key_[l] <= cur_key_[r]) { w[pos] = l; tree_[pos] = r; }
      else                            { w[pos] = r; tree_[pos] = l; }
    }
    tree_[0] = w[1];
  }

  void adjust(std::size_t winner) {
    std::size_t p = (K_ + winner) / 2;
    while (p >= 1) {
      if (cur_key_[tree_[p]] < cur_key_[winner])
        std::swap(tree_[p], winner);
      if (p == 1) break;
      p /= 2;
    }
    tree_[0] = winner;
  }

  std::vector<Source*>         sources_;
  std::vector<MarketDataEvent> fronts_;
  std::vector<bool>            exhausted_;
  std::vector<NanoTime>        cur_key_;
  std::vector<std::size_t>     tree_;
  std::size_t                  K_{0};
  bool                         primed_{false};
};

} // namespace cmf
