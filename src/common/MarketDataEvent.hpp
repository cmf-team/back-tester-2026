// MarketDataEvent - unit of input for the event-driven backtester.
//
// A MarketDataEvent is produced by the data ingestion layer for every raw
// L3/MBO message read from an exchange dump. The event carries all the fields
// the Limit Order Book (LOB) engine needs to replay the book, plus enough
// metadata (symbol, instrument_id, sequence, ...) to filter / diagnose / log.

#pragma once

#include "common/BasicTypes.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <ostream>
#include <string_view>

namespace cmf {

// Max symbol length in the raw L3 record (matches TOrderlog 'S45').
inline constexpr std::size_t kMaxSymbolLen = 45;

// One parsed L3/MBO message, ready to feed the LOB engine.
// Layout is kept trivial on purpose (no std::string) so the event can be
// copied / moved cheaply and later pooled in a ring buffer if needed.
struct MarketDataEvent {
  NanoTime ts_event = 0;  // exchange send time, ns since epoch
  NanoTime ts_recv = 0;   // our receive time,   ns since epoch

  Action action = Action::None;  // A / C / M / T / F / R
  Side side = Side::None;        // Buy / Sell / None

  Price price = 0.0;
  std::int64_t size = 0;

  OrderId order_id = 0;
  std::int32_t channel_id = 0;
  std::uint8_t flags = 0;
  std::int32_t ts_in_delta = 0;
  std::int32_t sequence = 0;

  // Fixed-size symbol buffer (null-padded); mirrors the 'S45' numpy field.
  std::array<char, kMaxSymbolLen> symbol{};

  std::int32_t rtype = 0;
  std::uint32_t publisher_id = 0;
  std::int32_t instrument_id = 0;

  // View of the symbol as a string_view (trims trailing nulls).
  [[nodiscard]] std::string_view symbolView() const noexcept {
    const auto *begin = symbol.data();
    std::size_t len = 0;
    while (len < symbol.size() && begin[len] != '\0') {
      ++len;
    }
    return {begin, len};
  }

  // Store a symbol string (truncates if longer than kMaxSymbolLen).
  void setSymbol(std::string_view s) noexcept {
    const std::size_t n = std::min(s.size(), symbol.size());
    symbol.fill('\0');
    std::memcpy(symbol.data(), s.data(), n);
  }
};

// Pretty printer used by the verification consumer and the summary block.
std::ostream &operator<<(std::ostream &os, const MarketDataEvent &ev);

} // namespace cmf
