#pragma once
#include "Order.hpp"
#include <cstdint>
#include <string>

enum class EventType : uint8_t
{
    Add,
    Cancel,
    Modify,
    Trade,
    Fill,
    Reset // action = R: clear all resting orders for the instrument
};

// Databento flags (bit field)
namespace Flags
{
constexpr uint8_t F_LAST = 1 << 7;     // last record in event group
constexpr uint8_t F_TOB = 1 << 6;      // top-of-book message
constexpr uint8_t F_SNAPSHOT = 1 << 5; // from replay/snapshot
constexpr uint8_t F_MBP = 1 << 4;      // aggregated price level, not individual order
constexpr uint8_t F_BAD_TS = 1 << 3;   // ts_recv unreliable
constexpr uint8_t F_BAD_BOOK = 1 << 2; // unrecoverable gap detected
} // namespace Flags

struct MarketDataEvent
{
    EventType type{};

    // ts_recv  — monotonic Databento receive timestamp. Used for sorting & merging.
    // ts_event — exchange-side timestamp, may be non-monotonic.
    uint64_t ts{}; // = ts_recv (index timestamp per Databento docs)
    uint64_t ts_event{};

    uint64_t order_id{};
    uint64_t instrument_id{};

    Side side{Side::Bid};
    int64_t price{}; // fixed-precision 1e-9: 5411750000000 == 5411.75
    int64_t qty{};

    uint8_t flags{};

    // Human-readable helpers
    double price_decimal() const { return price / 1e9; }
    bool is_mbp() const { return flags & Flags::F_MBP; }
    bool is_tob() const { return flags & Flags::F_TOB; }

    static const char* type_str(EventType t)
    {
        switch (t)
        {
        case EventType::Add:
            return "Add";
        case EventType::Cancel:
            return "Cancel";
        case EventType::Modify:
            return "Modify";
        case EventType::Trade:
            return "Trade";
        case EventType::Fill:
            return "Fill";
        case EventType::Reset:
            return "Reset";
        default:
            return "Unknown";
        }
    }
    static const char* side_str(Side s)
    {
        return s == Side::Bid ? "Bid" : "Ask";
    }
};
