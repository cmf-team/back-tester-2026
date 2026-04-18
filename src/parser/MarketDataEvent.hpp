// MarketDataEvent: a single L3 (MBO) message produced by the ingestion layer
// of the back-tester. Mirrors the fields delivered by Databento JSON-line feed
// (XEUR.EOBI / mbo schema) without coupling to the wire format itself.

#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>
#include <iosfwd>
#include <limits>
#include <string>
#include <string_view>

namespace cmf {

// Action carried by an L3 message (Databento MBO conventions).
//   A - Add a new resting order to the book
//   M - Modify an existing order
//   C - Cancel (full / partial) an existing order
//   R - Clear the book (start of session, snapshot reset)
//   T - Trade (does NOT change the book by itself)
//   F - Fill (matched against a resting order)
//   N - None / unspecified
enum class Action : char {
  None = 'N',
  Add = 'A',
  Modify = 'M',
  Cancel = 'C',
  Clear = 'R',
  Trade = 'T',
  Fill = 'F',
};

// Side as reported by the market-data feed: bid / ask / none.
// Distinct from cmf::Side which carries trade direction (Buy / Sell).
enum class MdSide : char {
  None = 'N',
  Bid = 'B',
  Ask = 'A',
};

// Sentinel for an undefined price (Databento reports null for snapshot resets).
inline constexpr Price kUndefinedPrice = std::numeric_limits<Price>::quiet_NaN();

inline bool priceDefined(Price p) noexcept {
  return !(p != p); // NaN-safe check
}

// Single immutable L3 message handed to the LOB / strategy layer.
//
// The fields explicitly required by HW-1 are first
// (timestamp / order_id / side / price / size / action), the rest are kept
// because they are essentially free to parse and useful downstream.
struct MarketDataEvent {
  // Required by the assignment ----------------------------------------------
  NanoTime timestamp{};   // ts_event in nanoseconds since epoch (UTC)
  OrderId order_id{};     // exchange-side order id
  MdSide side{MdSide::None};
  Price price{kUndefinedPrice};
  Quantity size{};
  Action action{Action::None};

  // Auxiliary fields parsed from the same record ----------------------------
  NanoTime ts_recv{};
  std::uint32_t instrument_id{};
  std::uint16_t publisher_id{};
  std::uint16_t channel_id{};
  std::uint32_t sequence{};
  std::int32_t ts_in_delta{};
  std::uint8_t rtype{};
  std::uint8_t flags{};
  std::string symbol{};
};

// Helpers ------------------------------------------------------------------

// Parse "YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ" into nanoseconds since UNIX epoch.
// Returns 0 on malformed input (caller may inspect via parseMarketDataLine
// return code).
NanoTime parseIso8601Nanos(std::string_view ts) noexcept;

// Formats a NanoTime back to the canonical ISO-8601 / nanosecond form used
// by Databento. Useful for printing summaries.
std::string formatIso8601Nanos(NanoTime ns);

// Map wire-level chars to enums. Unknown values become `None`.
Action parseAction(char c) noexcept;
MdSide parseMdSide(char c) noexcept;

// Stream insertion: prints the event in a fixed, human-readable format
// (used by the consumer in main()).
std::ostream &operator<<(std::ostream &os, const MarketDataEvent &ev);

} // namespace cmf
