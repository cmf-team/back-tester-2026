#include "market_data/MarketDataEvent.hpp"

#include <ostream>
#include <stdexcept>
#include <string>

namespace cmf {

MdAction parseAction(char c) {
  switch (c) {
  case 'A':
    return MdAction::Add;
  case 'M':
    return MdAction::Modify;
  case 'C':
    return MdAction::Cancel;
  case 'R':
    return MdAction::Clear;
  case 'T':
    return MdAction::Trade;
  case 'F':
    return MdAction::Fill;
  case 'N':
    return MdAction::None;
  default:
    throw std::invalid_argument(std::string{"invalid MD action: '"} + c + "'");
  }
}

Side parseSide(char c) {
  switch (c) {
  case 'B':
    return Side::Buy;
  case 'A':
    return Side::Sell;
  case 'N':
    return Side::None;
  default:
    throw std::invalid_argument(std::string{"invalid MD side: '"} + c + "'");
  }
}

bool operator<(const MarketDataEvent &a, const MarketDataEvent &b) noexcept {
  if (a.ts_recv != b.ts_recv)
    return a.ts_recv < b.ts_recv;
  if (a.sequence != b.sequence)
    return a.sequence < b.sequence;
  return a.instrument_id < b.instrument_id;
}

bool operator>(const MarketDataEvent &a, const MarketDataEvent &b) noexcept {
  return b < a;
}

std::ostream &operator<<(std::ostream &os, const MarketDataEvent &e) {
  os << "ts_recv=" << e.ts_recv << " ts_event=" << e.ts_event
     << " order_id=" << e.order_id << " side=" << static_cast<int>(e.side)
     << " price=";
  if (e.priceDefined())
    os << e.priceAsDouble();
  else
    os << "UNDEF";
  os << " size=" << e.size << " action=" << static_cast<char>(e.action)
     << " instrument_id=" << e.instrument_id << " symbol=" << e.symbol;
  return os;
}

} // namespace cmf
