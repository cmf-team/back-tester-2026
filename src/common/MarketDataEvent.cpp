#include "MarketDataEvent.hpp"

const std::string MarketDataEvent::sideToString(Side s) noexcept
{
    switch (s)
    {
    case Side::Bid:
        return "Bid";
    case Side::Ask:
        return "Ask";
    case Side::None:
        return "None";
    }

    return "None";
}

const std::string MarketDataEvent::actionToString(Action a) noexcept
{
    switch (a)
    {
    case Action::Add:
        return "Add";
    case Action::Modify:
        return "Modify";
    case Action::Cancel:
        return "Cancel";
    case Action::Clear:
        return "Clear";
    case Action::Trade:
        return "Trade";
    case Action::Fill:
        return "Fill";
    case Action::None:
        return "None";
    }
    return "None";
}

const std::string MarketDataEvent::flagToString(Flag f) noexcept
{
    switch (f)
    {
    case Flag::None:
        return "None";
    case Flag::F_RESERVED:
        return "F_RESERVED";
    case Flag::F_PUBLISHER_SPECIFIC:
        return "F_PUBLISHER_SPECIFIC";
    case Flag::F_MAYBE_BAD_BOOK:
        return "F_MAYBE_BAD_BOOK";
    case Flag::F_BAD_TS_RECV:
        return "F_BAD_TS_RECV";
    case Flag::F_MBP:
        return "F_MBP";
    case Flag::F_SNAPSHOT:
        return "F_SNAPSHOT";
    case Flag::F_LAST:
        return "F_LAST";
    case Flag::F_TOB:
        return "F_TOB";
    }
    return "None";
}

const std::string MarketDataEvent::rTypeToString(RType r) noexcept
{
    switch (r)
    {
    case RType::MBP_0:
        return "MBP_0";
    case RType::MBP_1:
        return "MBP_1";
    case RType::MBP_10:
        return "MBP_10";
    case RType::Status:
        return "Status";
    case RType::Definition:
        return "Definition";
    case RType::Imbalance:
        return "Imbalance";
    case RType::Error:
        return "Error";
    case RType::SymbolMapping:
        return "SymbolMapping";
    case RType::System:
        return "System";
    case RType::Statistics:
        return "Statistics";
    case RType::OHLCV_1s:
        return "OHLCV_1s";
    case RType::OHLCV_1m:
        return "OHLCV_1m";
    case RType::OHLCV_1h:
        return "OHLCV_1h";
    case RType::OHLCV_1d:
        return "OHLCV_1d";
    case RType::MBO:
        return "MBO";
    case RType::CMBP_1:
        return "CMBP_1";
    case RType::CBBO_1s:
        return "CBBO_1s";
    case RType::CBBO_1m:
        return "CBBO_1m";
    case RType::TCBBO:
        return "TCBBO";
    case RType::BBO_1s:
        return "BBO_1s";
    case RType::BBO_1m:
        return "BBO_1m";
    }

    return "None";
};