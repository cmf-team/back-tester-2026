#pragma once

#include "common/BasicTypes.hpp"
#include <limits>

namespace cmf {

// Prices are fixed-precision int64: 1 unit = 1e-9
// e.g. 5411750000000 → 5411.75 in decimal
// UNDEF_PRICE (INT64_MAX) denotes a null/undefined price
constexpr int64_t UNDEF_PRICE = std::numeric_limits<int64_t>::max();

enum class Action : char {
  Add = 'A',    // Insert a new order into the book
  Modify = 'M', // Change an order's price and/or size
  Cancel = 'C', // Fully or partially cancel an order
  Clear = 'R',  // Remove all resting orders for the instrument
  Trade = 'T',  // Aggressing order traded; does not affect the book
  Fill = 'F',   // Resting order was filled; does not affect the book
  None = 'N',   // No action; may carry flags or other information
};

enum class RType : uint8_t {
  Mbp0 = 0x00,          // MBP-0  — Trades schema (book depth 0)
  Mbp1 = 0x01,          // MBP-1  — TBBO / MBP-1 schema
  Mbp10 = 0x0A,         // MBP-10 — book depth 10
  Status = 0x12,        // Exchange status record
  Definition = 0x13,    // Instrument definition record
  Imbalance = 0x14,     // Order imbalance record
  Error = 0x15,         // Error record (live gateway)
  SymbolMapping = 0x16, // Symbol mapping record (live gateway)
  System = 0x17,        // Non-error system record (live gateway)
  Statistics = 0x18,    // Statistics record from the publisher
  Ohlcv1s = 0x20,       // OHLCV at 1-second cadence
  Ohlcv1m = 0x21,       // OHLCV at 1-minute cadence
  Ohlcv1h = 0x22,       // OHLCV at hourly cadence
  Ohlcv1d = 0x23,       // OHLCV at daily cadence
  Mbo = 0xA0,           // Market-by-order record
  Cmbp1 = 0xB1,         // Consolidated MBP-1
  Cbbo1s = 0xC0,        // Consolidated BBO at 1-second cadence
  Cbbo1m = 0xC1,        // Consolidated BBO at 1-minute cadence
  Tcbbo = 0xC2,         // Consolidated BBO — trades only
  Bbo1s = 0xC3,         // BBO at 1-second cadence
  Bbo1m = 0xC4,         // BBO at 1-minute cadence
};

enum class Flags : uint8_t {
  None = 0x00,
  Last = 1u << 7,     // 128 — last record in event for this instrument_id
  Tob = 1u << 6,      //  64 — top-of-book message, not an individual order
  Snapshot = 1u << 5, //  32 — sourced from a replay / snapshot server
  Mbp = 1u << 4,      //  16 — aggregated price-level message
  BadTsRecv =
      1u << 3, //   8 — ts_recv inaccurate (clock issues / packet reordering)
  MaybeBadBook = 1u << 2,  //   4 — unrecoverable gap detected in channel
  PublisherSpec = 1u << 1, //   2 — publisher-specific (see dataset supplement)
  // bit 0: reserved for internal use, can safely be ignored
};

constexpr Flags operator|(Flags a, Flags b) noexcept {
  return static_cast<Flags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
constexpr Flags operator&(Flags a, Flags b) noexcept {
  return static_cast<Flags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
constexpr Flags operator~(Flags a) noexcept {
  return static_cast<Flags>(~static_cast<uint8_t>(a));
}
constexpr Flags &operator|=(Flags &a, Flags b) noexcept { return a = a | b; }
constexpr Flags &operator&=(Flags &a, Flags b) noexcept { return a = a & b; }

constexpr bool has_flag(Flags field, Flags bit) noexcept {
  return (field & bit) != Flags::None;
}

// ---------------------------------------------------------------------------
// MarketDataEvent
// ---------------------------------------------------------------------------
struct MarketDataEvent {
  NanoTime ts_recv = 0;
  NanoTime ts_event = 0;
  uint64_t order_id = 0;
  int64_t price =
      UNDEF_PRICE; // fixed-precision: 1 unit = 1e-9; INT64_MAX = undefined
  uint32_t instrument_id = 0;
  uint32_t publisher_id = 0;
  uint32_t sequence = 0;
  uint32_t size = 0;
  int32_t ts_in_delta =
      0; // nanoseconds between ts_recv and publisher send time
  uint16_t channel_id = 0;
  RType rtype = RType::Mbp0;
  Flags flags = Flags::None;
  Action action = Action::None;
  Side side = Side::None;

  auto operator<=>(const MarketDataEvent &other) const noexcept {
    return ts_recv <=> other.ts_recv;
  }

  [[nodiscard]] double price_as_double() const noexcept {
    return static_cast<double>(price) * 1e-9;
  }

  [[nodiscard]] bool is_price_defined() const noexcept {
    return price != UNDEF_PRICE;
  }
};

} // namespace cmf
