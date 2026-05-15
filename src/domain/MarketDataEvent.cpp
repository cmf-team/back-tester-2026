#include "domain/MarketDataEvent.hpp"

#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <tuple>

namespace md {

char toChar(Side side) {
    return static_cast<char>(side);
}

char toChar(Action action) {
    return static_cast<char>(action);
}

std::string sideName(Side side) {
    switch (side) {
        case Side::Ask:  return "Ask";
        case Side::Bid:  return "Bid";
        case Side::None: return "None";
    }

    return "Unknown";
}

std::string actionName(Action action) {
    switch (action) {
        case Action::Add:    return "Add";
        case Action::Modify: return "Modify";
        case Action::Cancel: return "Cancel";
        case Action::Clear:  return "Clear";
        case Action::Trade:  return "Trade";
        case Action::Fill:   return "Fill";
        case Action::None:   return "None";
    }

    return "Unknown";
}

std::string formatPrice(std::int64_t price) {
    if (price == std::numeric_limits<std::int64_t>::max()) {
        return "UNDEF";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(9)
        << static_cast<long double>(price) / 1'000'000'000.0L;

    return oss.str();
}

std::string formatEventFields(const MarketDataEvent& event) {
    std::ostringstream oss;
    oss << "timestamp=" << event.timestamp
        << ", order_id=" << event.order_id
        << ", side=" << toChar(event.side)
        << ", price=" << event.price
        << ", size=" << event.size
        << ", action=" << toChar(event.action);
    return oss.str();
}

bool eventComesBefore(const MarketDataEvent& lhs, const MarketDataEvent& rhs) {
    return std::tie(lhs.timestamp, lhs.source_file_id, lhs.source_sequence)
         < std::tie(rhs.timestamp, rhs.source_file_id, rhs.source_sequence);
}

std::ostream& operator<<(std::ostream& os, const MarketDataEvent& event) {
    os << "MarketDataEvent{"
       << "timestamp=" << event.timestamp
       << ", ts_recv=" << event.ts_recv
       << ", ts_event=" << event.ts_event
       << ", order_id=" << event.order_id
       << ", side=" << toChar(event.side)
       << ", price=" << event.price
       << ", size=" << event.size
       << ", action=" << toChar(event.action)
       << ", instrument_id=" << event.instrument_id
       << ", source_file_id=" << event.source_file_id
       << ", source_sequence=" << event.source_sequence
       << "}";

    return os;
}

} // namespace md
