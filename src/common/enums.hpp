#pragma once
#include <stdexcept>
#include <string>

namespace cmf::action {
enum class Action : char {
    Add    = 'A', // Insert a new order into the book
    Modify = 'M', // Change price and/or size
    Cancel = 'C', // Fully or partially cancel
    Clear  = 'R', // Remove all resting orders
    Trade  = 'T', // Aggressing order traded (no book change)
    Fill   = 'F', // Resting order filled (no book change)
    None   = 'N'  // No action
};

inline Action from_char(char c) {
    switch (c) {
        case 'A': return Action::Add;
        case 'M': return Action::Modify;
        case 'C': return Action::Cancel;
        case 'R': return Action::Clear;
        case 'T': return Action::Trade;
        case 'F': return Action::Fill;
        case 'N': return Action::None;
        default: throw std::invalid_argument("Unknown action");
    }
}

inline std::string to_string(Action a) {
    switch (a) {
        case Action::Add:    return "Add";
        case Action::Modify: return "Modify";
        case Action::Cancel: return "Cancel";
        case Action::Clear:  return "Clear";
        case Action::Trade:  return "Trade";
        case Action::Fill:   return "Fill";
        case Action::None:   return "None";
        default:             return "Unknown";
    }
}
}

namespace cmf::flags {
    enum Flag : std::uint8_t {
        F_LAST              = 1 << 7,  // last record in event for this instrument_id
        F_TOB               = 1 << 6,  // top-of-book message, not individual order
        F_SNAPSHOT          = 1 << 5,  // sourced from replay / snapshot server
        F_MBP               = 1 << 4,  // aggregated price level, not individual order
        F_BAD_TS_RECV       = 1 << 3,  // ts_recv inaccurate (clock issue / reordering)
        F_MAYBE_BAD_BOOK    = 1 << 2,  // unrecoverable gap — book state unknown
        F_PUBLISHER_SPECIFIC= 1 << 1,  // meaning depends on publisher_id
    };

    inline bool has(std::uint8_t f, Flag flag) { return f & flag; }

    // Returns true when the row should be discarded:
    //   F_BAD_TS_RECV    — timestamp is unreliable
    //   F_MAYBE_BAD_BOOK — channel gap, book is in unknown state
    inline bool should_skip(std::uint8_t f) {
        return has(f, F_BAD_TS_RECV) || has(f, F_MAYBE_BAD_BOOK);
    }
}

namespace cmf::side {
    enum class Side : signed short { None = 0, Buy = 1, Sell = -1 };

    inline Side from_char(char c) {
        switch (c) {
            case 'A': return Side::Sell;
            case 'B': return Side::Buy;
            case 'N': return Side::None;
            default: throw std::invalid_argument("Unknown Side");
        }
    }

    inline const char* to_string(Side s) {
        switch (s) {
            case Side::Sell: return "Sell";
            case Side::Buy:  return "Buy";
            case Side::None: return "None";
            default:         return "Unknown";
        }
    }
}