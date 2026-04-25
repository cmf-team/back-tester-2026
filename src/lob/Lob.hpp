// Single-instrument limit order book.
//
// Design: tick-bucket bitmap. Each side stores a fixed-size `levels` array
// indexed by tick offset from `price_floor_`, plus a 2-level bitmap marking
// populated levels. Best bid/ask are one CTZ/CLZ away — true O(1).
//
// Invariants:
//   bit(bucket) set <=> levels[bucket].order_count > 0 <=> volume > 0
//   summary bit(w) set <=> words[w] != 0
//   sum_over_orders_with(bucket, side) { size } == levels[bucket].volume

#pragma once

#include "common/BasicTypes.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <unordered_map>

namespace cmf {

struct LobConfig {
  // tick_size in 1e-9 fixed point.
  //   0 (default)  -> auto-detect from the first priced event:
  //                   tick_size = min(largest_pow10_divisor(first_price),
  //                                   auto_tick_max).
  //   > 0          -> explicit (e.g. 10'000'000 == $0.01).
  int64_t tick_size{0};

  // Cap for the auto-detected tick. Ignored when tick_size > 0.
  // $0.01 is fine-grained enough for typical equity/futures feeds and prevents
  // round prices (e.g. $100.00 anchors at $100 ticks) from being unusable.
  int64_t auto_tick_max{10'000'000};
};

class Lob {
 public:
  // 4096 = 64 words * 64 bits/word. The summary fits in a single uint64_t,
  // which lets bestBid / bestAsk be two bitscan instructions. Increasing this
  // past 64*64 would require a 3-level bitmap — keep as-is until proven small.
  static constexpr std::size_t kNumBuckets = 4096;
  static constexpr std::size_t kWordBits   = 64;
  static constexpr std::size_t kNumWords   = kNumBuckets / kWordBits;
  static_assert(kNumWords <= kWordBits,
                "2-level bitmap requires kNumWords <= 64 (single summary word)");

  struct PriceLevel {
    uint64_t volume{0};
    uint32_t order_count{0};
    // 4 bytes trailing padding — keep PriceLevel at 16 bytes.
  };

  explicit Lob(const uint32_t instrument_id = 0, const LobConfig cfg = {}) noexcept
      : cfg_(cfg), instrument_id_(instrument_id) {}

  // --- book mutators (hot path, inline below) ---
  void add(OrderId id, MdSide side, int64_t price, uint32_t size) noexcept;
  void modify(OrderId id, MdSide side, int64_t price, uint32_t size) noexcept;
  void cancel(OrderId id, uint32_t cancel_qty) noexcept;
  void clear() noexcept;

  // --- best bid/ask: O(1) via CLZ/CTZ ---
  bool hasBid() const noexcept { return bid_.summary != 0u; }
  bool hasAsk() const noexcept { return ask_.summary != 0u; }
  int64_t  bestBidPrice()  const noexcept;
  int64_t  bestAskPrice()  const noexcept;
  uint64_t bestBidVolume() const noexcept;
  uint64_t bestAskVolume() const noexcept;

  uint64_t volumeAt(MdSide side, int64_t price) const noexcept;

  // --- diagnostics / test hooks ---
  std::size_t bidLevels()  const noexcept { return bid_.level_count; }
  std::size_t askLevels()  const noexcept { return ask_.level_count; }
  std::size_t orderCount() const noexcept { return orders_.size(); }
  uint32_t instrumentId()  const noexcept { return instrument_id_; }
  bool     anchored()      const noexcept { return anchored_; }
  int64_t  priceFloor()    const noexcept { return price_floor_; }
  const LobConfig& config() const noexcept { return cfg_; }

  uint64_t oorWarnings()          const noexcept { return oor_warnings_; }
  uint64_t underflowWarnings()    const noexcept { return underflow_warnings_; }
  uint64_t missingOrderWarnings() const noexcept { return missing_order_warnings_; }

  void printSnapshot(std::ostream& os, std::size_t depth = 10) const;

 private:
  struct OrderHandle {
    uint32_t bucket;
    uint32_t size;
    MdSide   side;
  };

  struct BookSide {
    PriceLevel  levels[kNumBuckets]{};
    uint64_t    words[kNumWords]{};  // level-0: bit per bucket
    uint64_t    summary{0};          // level-1: bit per word
    std::size_t level_count{0};
  };

  // --- hot helpers (inline) ---
  bool priceToBucket(int64_t price, uint32_t& out) const noexcept {
    if (!anchored_) return false;
    const int64_t delta = price - price_floor_;
    if (delta < 0) return false;
    const int64_t idx = delta / cfg_.tick_size;
    if (idx >= static_cast<int64_t>(kNumBuckets)) return false;
    out = static_cast<uint32_t>(idx);
    return true;
  }

  int64_t bucketToPrice(uint32_t bucket) const noexcept {
    return price_floor_ + static_cast<int64_t>(bucket) * cfg_.tick_size;
  }

  static void setBit(BookSide& s, uint32_t bucket) noexcept {
    const uint32_t w = bucket >> 6;
    const uint32_t b = bucket & 63u;
    s.words[w] |= (uint64_t{1} << b);
    s.summary  |= (uint64_t{1} << w);
  }
  static void clearBit(BookSide& s, uint32_t bucket) noexcept {
    const uint32_t w = bucket >> 6;
    const uint32_t b = bucket & 63u;
    s.words[w] &= ~(uint64_t{1} << b);
    if (s.words[w] == 0u) s.summary &= ~(uint64_t{1} << w);
  }

  BookSide&       sideOf(MdSide s)       noexcept { return s == MdSide::Bid ? bid_ : ask_; }
  const BookSide& sideOf(MdSide s) const noexcept { return s == MdSide::Bid ? bid_ : ask_; }

  void anchorIfNeeded(int64_t price) noexcept {
    if (anchored_) return;
    doAnchor(price);
  }

  // --- cold helpers (.cpp) ---
  void doAnchor(int64_t price) noexcept;
  void warnOor(int64_t price) noexcept;
  void warnUnderflow(OrderId id, uint32_t have, uint32_t req) noexcept;
  void warnMissing(OrderId id) noexcept;

  // --- state ---
  LobConfig cfg_;
  int64_t   price_floor_{0};
  bool      anchored_{false};

  BookSide bid_, ask_;

  std::unordered_map<OrderId, OrderHandle> orders_;

  uint32_t instrument_id_;

  uint64_t oor_warnings_{0};
  uint64_t underflow_warnings_{0};
  uint64_t missing_order_warnings_{0};
  bool     warned_oor_{false};
  bool     warned_underflow_{false};
  bool     warned_missing_{false};
};

// ---- inline hot-path definitions (kept in header for driver-loop inlining) ----

inline void Lob::add(OrderId id, MdSide side, int64_t price, uint32_t size) noexcept {
  if (side == MdSide::None || size == 0) return;
  anchorIfNeeded(price);
  uint32_t bucket;
  if (!priceToBucket(price, bucket)) { warnOor(price); return; }

  auto& s   = sideOf(side);
  auto& lvl = s.levels[bucket];
  if (lvl.order_count == 0) {
    setBit(s, bucket);
    ++s.level_count;
  }
  lvl.volume += size;
  ++lvl.order_count;
  orders_.insert_or_assign(id, OrderHandle{bucket, size, side});
}

inline void Lob::modify(OrderId id, MdSide side, int64_t price,
                        uint32_t new_size) noexcept {
  if (side == MdSide::None) return;
  auto it = orders_.find(id);
  if (it == orders_.end()) {
    // Never-seen order: treat as Add so we stay consistent with the wire.
    add(id, side, price, new_size);
    return;
  }
  anchorIfNeeded(price);
  uint32_t new_bucket;
  if (!priceToBucket(price, new_bucket)) { warnOor(price); return; }

  auto& h = it->second;

  // Fast path: price+side unchanged — delta-update volume only.
  if (h.side == side && h.bucket == new_bucket) {
    auto& lvl = sideOf(side).levels[new_bucket];
    if (new_size >= h.size) lvl.volume += (new_size - h.size);
    else                    lvl.volume -= (h.size - new_size);
    h.size = new_size;
    return;
  }

  // Move: drop from old slot, insert into new.
  {
    auto& old_s   = sideOf(h.side);
    auto& old_lvl = old_s.levels[h.bucket];
    old_lvl.volume -= h.size;
    if (--old_lvl.order_count == 0) {
      clearBit(old_s, h.bucket);
      --old_s.level_count;
    }
  }
  auto& new_s   = sideOf(side);
  auto& new_lvl = new_s.levels[new_bucket];
  if (new_lvl.order_count == 0) {
    setBit(new_s, new_bucket);
    ++new_s.level_count;
  }
  new_lvl.volume += new_size;
  ++new_lvl.order_count;
  h = OrderHandle{new_bucket, new_size, side};
}

inline void Lob::cancel(OrderId id, uint32_t cancel_qty) noexcept {
  auto it = orders_.find(id);
  if (it == orders_.end()) { warnMissing(id); return; }

  auto& h   = it->second;
  auto& s   = sideOf(h.side);
  auto& lvl = s.levels[h.bucket];

  if (cancel_qty < h.size) {
    // partial: order still resting, bitmap unchanged
    h.size     -= cancel_qty;
    lvl.volume -= cancel_qty;
    return;
  }
  // full (including over-cancel). Clamp to handle.size so volume stays sane.
  if (cancel_qty > h.size) warnUnderflow(id, h.size, cancel_qty);
  lvl.volume -= h.size;
  if (--lvl.order_count == 0) {
    clearBit(s, h.bucket);
    --s.level_count;
  }
  orders_.erase(it);
}

inline int64_t Lob::bestBidPrice() const noexcept {
  if (bid_.summary == 0u) return MarketDataEvent::kUndefPrice;
  const int w = 63 - __builtin_clzll(bid_.summary);
  const int b = 63 - __builtin_clzll(bid_.words[w]);
  return bucketToPrice(static_cast<uint32_t>(w * 64 + b));
}

inline int64_t Lob::bestAskPrice() const noexcept {
  if (ask_.summary == 0u) return MarketDataEvent::kUndefPrice;
  const int w = __builtin_ctzll(ask_.summary);
  const int b = __builtin_ctzll(ask_.words[w]);
  return bucketToPrice(static_cast<uint32_t>(w * 64 + b));
}

inline uint64_t Lob::bestBidVolume() const noexcept {
  if (bid_.summary == 0u) return 0;
  const int w = 63 - __builtin_clzll(bid_.summary);
  const int b = 63 - __builtin_clzll(bid_.words[w]);
  return bid_.levels[w * 64 + b].volume;
}

inline uint64_t Lob::bestAskVolume() const noexcept {
  if (ask_.summary == 0u) return 0;
  const int w = __builtin_ctzll(ask_.summary);
  const int b = __builtin_ctzll(ask_.words[w]);
  return ask_.levels[w * 64 + b].volume;
}

inline uint64_t Lob::volumeAt(MdSide side, int64_t price) const noexcept {
  if (side == MdSide::None) return 0;
  uint32_t bucket;
  if (!priceToBucket(price, bucket)) return 0;
  return sideOf(side).levels[bucket].volume;
}

}  // namespace cmf
