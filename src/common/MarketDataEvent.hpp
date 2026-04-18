#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>
#include <ostream>
#include <string>

namespace cmf {

// Order event actions from Databento MBO feed
// Maps to the single-char 'action' field in the NDJSON data
enum class Action : char {
  Add = 'A',    // new order inserted into the book
  Modify = 'M', // order price and/or size changed
  Cancel = 'C', // order fully or partially cancelled
  Clear = 'R',  // all resting orders for the instrument removed
  Trade = 'T',  // aggressing order traded (does not affect book)
  Fill = 'F',   // resting order filled (does not affect book)
  None = 'N'    // no book effect, may carry flags or info
};

inline std::ostream &operator<<(std::ostream &os, Action a) {
  switch (a) {
  case Action::Add:
    return os << "Add";
  case Action::Modify:
    return os << "Modify";
  case Action::Cancel:
    return os << "Cancel";
  case Action::Clear:
    return os << "Clear";
  case Action::Trade:
    return os << "Trade";
  case Action::Fill:
    return os << "Fill";
  case Action::None:
    return os << "None";
  }
  return os << "Unknown";
}

inline std::ostream &operator<<(std::ostream &os, Side s) {
  switch (s) {
  case Side::Buy:
    return os << "Buy";
  case Side::Sell:
    return os << "Sell";
  case Side::None:
    return os << "None";
  }
  return os << "Unknown";
}

// A single L3 market-by-order event parsed from a Databento NDJSON record.
// One line of the daily file becomes one MarketDataEvent.
struct MarketDataEvent {
  NanoTime tsRecv{};  // Databento receive timestamp (index timestamp)
  NanoTime tsEvent{}; // exchange event timestamp

  Action action{Action::None};
  Side side{Side::None};

  Price price{};   // order price (0 or NaN when null in source)
  Quantity size{}; // order quantity

  OrderId orderId{}; // exchange-assigned order id

  std::uint32_t instrumentId{}; // Databento instrument id (unique per day)
  std::uint16_t channelId{};    // exchange channel / partition
  std::uint16_t publisherId{};  // Databento publisher id
  std::uint8_t flags{};         // bitfield (F_LAST, F_TOB, F_SNAPSHOT, etc.)
  std::uint8_t rtype{};         // record type (160 = MBO)

  std::int32_t tsInDelta{}; // nanoseconds: ts_recv - publisher send time
  std::uint32_t sequence{}; // exchange sequence number

  std::string symbol; // human-readable instrument symbol
};

inline std::ostream &operator<<(std::ostream &os, const MarketDataEvent &e) {
  os << "ts_recv=" << e.tsRecv << " ts_event=" << e.tsEvent
     << " action=" << e.action << " side=" << e.side << " price=" << e.price
     << " size=" << e.size << " order_id=" << e.orderId
     << " symbol=" << e.symbol;
  return os;
}

} // namespace cmf
