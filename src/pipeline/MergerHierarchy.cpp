#include "pipeline/MergerHierarchy.hpp"

#include <utility>

namespace cmf {

namespace {
std::size_t nextPow2(std::size_t n) noexcept {
  std::size_t p = 1;
  while (p < n)
    p *= 2;
  return p;
}
} // namespace

MergerHierarchy::MergerHierarchy(std::vector<EventSourcePtr> sources)
    : sources_(std::move(sources)) {
  // Always need at least one leaf so the tree array is non-empty.
  leaves_ = nextPow2(sources_.empty() ? 1 : sources_.size());
  buffered_.resize(leaves_);
  ts_.assign(leaves_, kExhausted);
  tree_.assign(2 * leaves_ - 1, 0);

  // Prime real sources; padded leaves stay at kExhausted.
  for (std::size_t i = 0; i < sources_.size(); ++i)
    refill(i);

  // Build the tree bottom-up. Initialise leaves first ...
  for (std::size_t i = 0; i < leaves_; ++i)
    tree_[leafSlot(i)] = i;
  // ... then internal nodes from the deepest to the root.
  if (leaves_ > 1) {
    for (std::size_t i = leaves_ - 2; ; --i) {
      tree_[i] = winner(tree_[2 * i + 1], tree_[2 * i + 2]);
      if (i == 0)
        break;
    }
  }
}

std::size_t MergerHierarchy::leafSlot(std::size_t leaf) const noexcept {
  return (leaves_ - 1) + leaf;
}

void MergerHierarchy::refill(std::size_t leaf) {
  if (leaf >= sources_.size()) {
    ts_[leaf] = kExhausted;
    return;
  }
  if (!sources_[leaf]->next(buffered_[leaf])) {
    ts_[leaf] = kExhausted;
    return;
  }
  ts_[leaf] = buffered_[leaf].timestamp;
}

std::size_t MergerHierarchy::winner(std::size_t a,
                                    std::size_t b) const noexcept {
  // Tie-break by leaf id keeps merges deterministic.
  if (ts_[a] < ts_[b]) return a;
  if (ts_[b] < ts_[a]) return b;
  return a < b ? a : b;
}

void MergerHierarchy::replayUp(std::size_t leaf) {
  std::size_t node = leafSlot(leaf);
  while (node != 0) {
    std::size_t parent  = (node - 1) / 2;
    std::size_t left    = 2 * parent + 1;
    std::size_t right   = 2 * parent + 2;
    tree_[parent] = winner(tree_[left], tree_[right]);
    node = parent;
  }
}

bool MergerHierarchy::next(MarketDataEvent &out) {
  const std::size_t winner_leaf = tree_[0];
  if (ts_[winner_leaf] == kExhausted)
    return false;
  out = std::move(buffered_[winner_leaf]);
  refill(winner_leaf);
  replayUp(winner_leaf);
  ++emitted_;
  return true;
}

} // namespace cmf
