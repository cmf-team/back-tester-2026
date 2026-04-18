// A single parsed Databento MBO market-data event.

#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>
#include <limits>
#include <string>

namespace cmf {

// Event type per Databento standards: A/M/C/R/T/F/N.
enum class Action : char {
  None   = 'N',
  Add    = 'A',
  Modify = 'M',
  Cancel = 'C',
  Clear  = 'R',
  Trade  = 'T',
  Fill   = 'F',
};

// Feed-side marker as encoded on the wire (Ask/Bid/None).
// Kept separate from cmf::Side which carries Buy/Sell trading semantics.
enum class MdSide : char {
  None = 'N',
  Ask  = 'A',
  Bid  = 'B',
};

struct MarketDataEvent {
  // Fixed-point prices: 1 unit == 1e-9. Matches DBN on-wire representation.
  static constexpr int64_t kPriceScale = 1'000'000'000LL;
  static constexpr int64_t kUndefPrice = (std::numeric_limits<int64_t>::max)();

  NanoTime    ts_recv{};
  NanoTime    ts_event{};
  int64_t     price{kUndefPrice};
  OrderId     order_id{};
  uint32_t    size{};
  uint32_t    sequence{};
  int32_t     ts_in_delta{};
  uint32_t    instrument_id{};
  uint16_t    publisher_id{};
  uint16_t    channel_id{};
  uint8_t     rtype{};
  uint8_t     flags{};
  Action      action{Action::None};
  MdSide      side{MdSide::None};
  std::string symbol{};   // owned copy; independent of the parser's mmap lifetime

  bool   priceDefined() const noexcept { return price != kUndefPrice; }
  double priceAsDouble() const noexcept {
    return priceDefined()
             ? static_cast<double>(price) / static_cast<double>(kPriceScale)
             : std::numeric_limits<double>::quiet_NaN();
  }
};

} // namespace cmf
