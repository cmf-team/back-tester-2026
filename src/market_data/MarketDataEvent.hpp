// MarketDataEvent — single L3 (MBO) message consumed from the feed.

#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <string>

namespace cmf {

// Action codes from Databento MBO schema.
// See: agent-info/documentation.md §Action.
enum class MdAction : char {
  None = 'N',
  Add = 'A',
  Modify = 'M',
  Cancel = 'C',
  Clear = 'R',
  Trade = 'T',
  Fill = 'F',
};

// Parses a one-character action code. Throws std::invalid_argument on unknown.
MdAction parseAction(char c);

// Parses a one-character side code: 'B' -> Buy, 'A' -> Sell, 'N' -> None.
// Throws std::invalid_argument on unknown.
Side parseSide(char c);

// Market data event — one L3 (MBO) message from the feed.
// Sorted chronologically by (ts_recv, sequence, instrument_id) for k-way merge.
struct MarketDataEvent {
  // Fixed-point price: 1 unit == 1e-9. Matches DBN on-wire representation
  // and avoids per-event std::stod / double parsing on the hot path.
  static constexpr std::int64_t kPriceScale = 1'000'000'000LL;
  static constexpr std::int64_t kUndefPrice =
      (std::numeric_limits<std::int64_t>::max)();

  NanoTime ts_recv{0};  // primary key for global chronological order
  NanoTime ts_event{0}; // matching engine timestamp
  std::int32_t ts_in_delta{0};
  std::uint32_t sequence{0};
  std::uint64_t instrument_id{0};
  std::uint16_t publisher_id{0};
  std::uint16_t flags{0};
  std::uint8_t rtype{0};
  std::uint8_t channel_id{0};
  MdAction action{MdAction::None};
  Side side{Side::None};
  OrderId order_id{0};
  std::int64_t price{kUndefPrice};
  std::uint32_t size{0};
  std::string symbol;

  bool priceDefined() const noexcept { return price != kUndefPrice; }

  // Converts the fixed-point price back to a double for display or use by
  // float-centric downstream code. NaN when undefined.
  double priceAsDouble() const noexcept {
    return priceDefined()
               ? static_cast<double>(price) /
                     static_cast<double>(kPriceScale)
               : std::numeric_limits<double>::quiet_NaN();
  }
};

// Strict weak ordering by (ts_recv, sequence, instrument_id).
bool operator<(const MarketDataEvent &a, const MarketDataEvent &b) noexcept;
bool operator>(const MarketDataEvent &a, const MarketDataEvent &b) noexcept;

// Human-readable one-line dump for verification output.
std::ostream &operator<<(std::ostream &os, const MarketDataEvent &e);

} // namespace cmf
