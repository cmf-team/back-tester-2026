#pragma once

#include <cstdint>
#include <string>

namespace domain::events {

// Fixed-precision sentinel for a null/undefined price in JSON (1 unit = 1e-9).
inline constexpr std::int64_t UNDEF_PRICE{9223372036854775807LL};

// Common header present in each DBN record.
struct MdHeader {
  // Matching-engine-received timestamp in nanoseconds since UNIX epoch.
  std::uint64_t ts_event;
  // Unsigned 8-bit record type discriminant (MBO is 160 / 0xA0).
  std::uint8_t rtype;
  // Databento publisher identifier (unsigned 16-bit integer).
  std::uint16_t publisher_id;
  // Venue instrument identifier (unsigned 32-bit integer).
  std::uint32_t instrument_id;
};

struct MarketDataEvent {
  // Databento receive timestamp exactly as it appears in the JSON.
  std::string ts_recv;
  MdHeader hd;
  // Order event action ('A','M','C','R','T','F','N').
  char action;
  // Side indicator ('A' ask, 'B' bid, 'N' none).
  char side;
  // Fixed-precision price (1 unit = 1e-9); UNDEF_PRICE denotes null JSON.
  std::int64_t price;
  // Order quantity (unsigned 32-bit integer).
  std::uint32_t size;
  // Databento-assigned channel identifier (unsigned 8-bit integer).
  std::uint8_t channel_id;
  // Venue-assigned order identifier (unsigned 64-bit integer).
  std::uint64_t order_id;
  // Bit flags with record/message metadata.
  std::uint8_t flags;
  // ts_recv - publisher_send_ts in nanoseconds (signed 32-bit integer).
  std::int32_t ts_in_delta;
  // Venue sequence number (unsigned 32-bit integer).
  std::uint32_t sequence;
  // Symbol string included in JSON outputs (not part of raw DBN MboMsg).
  std::string symbol;
};

const MarketDataEvent EOF_EVENT{
    .ts_recv = {},
    .hd = {},
    .action = {},
    .side = {},
    .price = {},
    .size = {},
    .channel_id = {},
    .order_id = {},
    .flags = {},
    .ts_in_delta = {},
    .sequence = {},
    .symbol = "EOF",
};

} // namespace domain::events
