// MergerHierarchy: k-way merge implemented as a binary "winner tree"
// (a.k.a. tournament tree).
//
// Layout:
//   * Number of input sources k may be arbitrary; we pad it up to the next
//     power of two N. Padded slots are always exhausted (timestamp = max).
//   * The tree is stored in an array of size 2N - 1.
//       leaves           : indices [N - 1 .. 2N - 2]
//       internal nodes   : indices [0 .. N - 2]   (root is 0)
//       parent(i)        = (i - 1) / 2
//       left(i)/right(i) = 2i + 1 / 2i + 2
//   * Each node holds the leaf index of the "winner" of its subtree
//     (the leaf with the smallest currently-buffered timestamp).
//
// Per emitted event we:
//   1) take winner from the root,
//   2) refill that leaf (pull next from the corresponding source),
//   3) walk from that leaf to the root replaying matches between the
//      walking node and its sibling -- O(log N) compares.
//
// This is asymptotically the same as the heap-based MergerFlat, but each
// step is a single comparison against the cached sibling instead of a full
// sift-down, which is why a winner tree typically wins on real workloads
// at moderate-to-large fan-in.

#pragma once

#include "pipeline/IEventSource.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace cmf {

class MergerHierarchy final : public IEventSource {
public:
  explicit MergerHierarchy(std::vector<EventSourcePtr> sources);

  bool next(MarketDataEvent &out) override;

  std::size_t   sources()    const noexcept { return sources_.size(); }
  std::size_t   leafCount()  const noexcept { return leaves_; }
  std::uint64_t emitted()    const noexcept { return emitted_; }

private:
  // Sentinel for "this leaf has no more events" -- always loses the match.
  static constexpr NanoTime kExhausted = std::numeric_limits<NanoTime>::max();

  std::vector<EventSourcePtr>  sources_;
  std::vector<MarketDataEvent> buffered_; // size = leaves_
  std::vector<NanoTime>        ts_;       // size = leaves_, kExhausted == done
  std::vector<std::size_t>     tree_;     // size = 2*leaves_ - 1
  std::size_t                  leaves_{0};

  std::uint64_t emitted_{0};

  // Pull the next event from source[leaf] into buffered_/ts_.
  void refill(std::size_t leaf);
  // Index of `leaf`'s slot in tree_ (i.e. its array position as a leaf node).
  std::size_t leafSlot(std::size_t leaf) const noexcept;
  // Replay the matches from a specific leaf up to the root.
  void replayUp(std::size_t leaf);
  // Pick the leaf with the smaller timestamp (with stable tie-break by leaf id).
  std::size_t winner(std::size_t a, std::size_t b) const noexcept;
};

} // namespace cmf
