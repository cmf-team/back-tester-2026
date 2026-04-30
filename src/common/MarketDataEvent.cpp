#include "MarketDataEvent.hpp"
#include "MarketDataParser.hpp"

auto operator<<(std::ostream& os, const Side& s) -> std::ostream&
{
    switch (s)
    {
    case Side::Bid:
        os << "Bid";
        break;
    case Side::Ask:
        os << "Ask";
        break;
    case Side::None:
        os << "None";
        break;
    }
    return os;
}

auto operator<<(std::ostream& os, const Action& a) -> std::ostream&
{
    switch (a)
    {
    case Action::Add:
        os << "Add";
        break;
    case Action::Modify:
        os << "Modify";
        break;
    case Action::Cancel:
        os << "Cancel";
        break;
    case Action::Clear:
        os << "Clear";
        break;
    case Action::Trade:
        os << "Trade";
        break;
    case Action::Fill:
        os << "Fill";
        break;
    case Action::None:
        os << "None";
        break;
    }
    return os;
}

auto operator<<(std::ostream& os, const Flag& f) -> std::ostream&
{
    switch (f)
    {
    case Flag::None:
        os << "None";
        break;
    case Flag::Reserved:
        os << "F_RESERVED";
        break;
    case Flag::PublisherSpecific:
        os << "F_PUBLISHER_SPECIFIC";
        break;
    case Flag::MaybeBadBook:
        os << "F_MAYBE_BAD_BOOK";
        break;
    case Flag::BadTsRecv:
        os << "F_BAD_TS_RECV";
        break;
    case Flag::Mbp:
        os << "F_MBP";
        break;
    case Flag::Snapshot:
        os << "F_SNAPSHOT";
        break;
    case Flag::Tob:
        os << "F_TOB";
        break;
    case Flag::Last:
        os << "F_LAST";
        break;
    }
    return os;
}

auto operator<<(std::ostream& os, const RType& r) -> std::ostream&
{
    switch (r)
    {
    case RType::Mbp0:
        os << "MBP_0";
        break;
    case RType::Mbp1:
        os << "MBP_1";
        break;
    case RType::Mbp10:
        os << "MBP_10";
        break;
    case RType::Status:
        os << "Status";
        break;
    case RType::Definition:
        os << "Definition";
        break;
    case RType::Imbalance:
        os << "Imbalance";
        break;
    case RType::Error:
        os << "Error";
        break;
    case RType::SymbolMapping:
        os << "SymbolMapping";
        break;
    case RType::System:
        os << "System";
        break;
    case RType::Statistics:
        os << "Statistics";
        break;
    case RType::Ohlcv1s:
        os << "OHLCV_1s";
        break;
    case RType::Ohlcv1m:
        os << "OHLCV_1m";
        break;
    case RType::Ohlcv1h:
        os << "OHLCV_1h";
        break;
    case RType::Ohlcv1d:
        os << "OHLCV_1d";
        break;
    case RType::Mbo:
        os << "MBO";
        break;
    case RType::Cmbp1:
        os << "CMBP_1";
        break;
    case RType::Cbbo1s:
        os << "CBBO_1s";
        break;
    case RType::Cbbo1m:
        os << "CBBO_1m";
        break;
    case RType::Tcbbo:
        os << "TCBBO";
        break;
    case RType::Bbo1s:
        os << "BBO_1s";
        break;
    case RType::Bbo1m:
        os << "BBO_1m";
        break;
    }
    return os;
}

auto operator<<(std::ostream& os, const MarketDataEvent& e) -> std::ostream&
{
    os << "ts_recv=" << nanosToIso(e.ts_recv) << ", "
       << " ts_event=" << nanosToIso(e.ts_event) << ", "
       << " rtype=" << e.rtype << ", "
       << " publisher=" << e.publisher_id << ", "
       << " instrument=" << e.instrument_id << ", "
       << " action=" << e.action << ", "
       << " side=" << e.side << ", "
       << " price=" << e.price << ", "
       << " size=" << e.size << ", "
       << " order_id=" << e.order_id << ", "
       << " flag=" << e.flag << ", "
       << " ts_in_delta=" << e.ts_in_delta;
    return os;
}
