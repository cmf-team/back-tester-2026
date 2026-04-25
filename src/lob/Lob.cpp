#include "lob/Lob.hpp"

#include <iomanip>
#include <iostream>
#include <ostream>

namespace cmf {

namespace {

// Largest power of 10 that divides `price`, capped at `cap`. Used to
// auto-derive a sensible tick from the very first observed price.
//   1'156'100'000 (=1.1561) cap=1e7  -> 100'000
//   100'000'000'000 (=$100.00)       -> capped to 1e7
//   1'234            (no trail zeros) -> 1
int64_t largestPow10Divisor(int64_t price, int64_t cap) noexcept {
  if (price <= 0) return 1;
  int64_t tick = 1;
  while (true) {
    const int64_t next = tick * 10;
    if (next > cap) break;
    if (price % next != 0) break;
    tick = next;
  }
  return tick;
}

}  // namespace

void Lob::clear() noexcept {
  std::memset(bid_.levels, 0, sizeof(bid_.levels));
  std::memset(ask_.levels, 0, sizeof(ask_.levels));
  std::memset(bid_.words,  0, sizeof(bid_.words));
  std::memset(ask_.words,  0, sizeof(ask_.words));
  bid_.summary = 0;
  ask_.summary = 0;
  bid_.level_count = 0;
  ask_.level_count = 0;
  orders_.clear();
  // Anchor is preserved: Clear wipes the book, not the price universe.
}

void Lob::doAnchor(int64_t price) noexcept {
  if (price == MarketDataEvent::kUndefPrice) return;

  // Auto-pick tick_size from the first priced event when the user didn't pin
  // one. Resolved value is stored back so config().tick_size is inspectable.
  if (cfg_.tick_size <= 0) {
    cfg_.tick_size = largestPow10Divisor(price, cfg_.auto_tick_max);
  }

  const int64_t half = static_cast<int64_t>(kNumBuckets / 2) * cfg_.tick_size;
  int64_t floor = price - half;

  // Optimization: when a non-negative first price would put the centered floor
  // below zero, slide the window up so the floor sits at 0. Negative prices
  // are vanishingly rare in real feeds (calendar-spread futures aside), so
  // their half of the window would otherwise be wasted. Price moves into the
  // lower half of the window; the upper half gains extra upside headroom.
  if (price >= 0 && floor < 0) {
    floor = 0;
  } else {
    // Round down to a multiple of tick_size; C++ signed % can go negative.
    int64_t rem = floor % cfg_.tick_size;
    if (rem < 0) rem += cfg_.tick_size;
    floor -= rem;
  }

  price_floor_ = floor;
  anchored_ = true;
}

void Lob::printSnapshot(std::ostream& os, std::size_t depth) const {
  const auto prev_flags = os.flags();
  const auto prev_prec  = os.precision();

  const auto printLevel = [&](const PriceLevel& lvl, uint32_t bucket) {
    const int64_t px = bucketToPrice(bucket);
    os << "  " << std::fixed << std::setprecision(9)
       << (static_cast<double>(px) /
           static_cast<double>(MarketDataEvent::kPriceScale))
       << "  x  " << lvl.volume << "  (" << lvl.order_count << " ord)\n";
  };

  os << "inst=" << instrument_id_
     << "  levels[bid=" << bid_.level_count
     << ",ask="         << ask_.level_count
     << "]  orders="    << orders_.size() << '\n';

  // Walk bids from highest set bit downward.
  os << "  bids (top " << depth << "):\n";
  {
    uint64_t summary = bid_.summary;
    uint64_t words[kNumWords];
    std::memcpy(words, bid_.words, sizeof(words));
    for (std::size_t i = 0; i < depth && summary != 0u; ++i) {
      const int w = 63 - __builtin_clzll(summary);
      const int b = 63 - __builtin_clzll(words[w]);
      const uint32_t bucket = static_cast<uint32_t>(w * 64 + b);
      printLevel(bid_.levels[bucket], bucket);
      words[w] &= ~(uint64_t{1} << b);
      if (words[w] == 0u) summary &= ~(uint64_t{1} << w);
    }
  }

  // Walk asks from lowest set bit upward.
  os << "  asks (top " << depth << "):\n";
  {
    uint64_t summary = ask_.summary;
    uint64_t words[kNumWords];
    std::memcpy(words, ask_.words, sizeof(words));
    for (std::size_t i = 0; i < depth && summary != 0u; ++i) {
      const int w = __builtin_ctzll(summary);
      const int b = __builtin_ctzll(words[w]);
      const uint32_t bucket = static_cast<uint32_t>(w * 64 + b);
      printLevel(ask_.levels[bucket], bucket);
      words[w] &= ~(uint64_t{1} << b);
      if (words[w] == 0u) summary &= ~(uint64_t{1} << w);
    }
  }

  os.flags(prev_flags);
  os.precision(prev_prec);
}

void Lob::warnOor(int64_t price) noexcept {
  ++oor_warnings_;
  if (warned_oor_) return;
  warned_oor_ = true;
  const int64_t ceiling =
      price_floor_ + static_cast<int64_t>(kNumBuckets) * cfg_.tick_size;
  std::cerr << "[Lob] inst=" << instrument_id_ << " price " << price
            << " outside configured window [" << price_floor_ << ", "
            << ceiling << ")\n";
}

void Lob::warnUnderflow(OrderId id, uint32_t have, uint32_t req) noexcept {
  ++underflow_warnings_;
  if (warned_underflow_) return;
  warned_underflow_ = true;
  std::cerr << "[Lob] inst=" << instrument_id_
            << " cancel underflow: order_id=" << id
            << " resting=" << have << " cancel=" << req
            << " (clamped to full cancel)\n";
}

void Lob::warnMissing(OrderId id) noexcept {
  ++missing_order_warnings_;
  if (warned_missing_) return;
  warned_missing_ = true;
  std::cerr << "[Lob] inst=" << instrument_id_
            << " cancel for unknown order_id=" << id << '\n';
}

}  // namespace cmf
