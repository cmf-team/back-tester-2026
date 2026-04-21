#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <ostream>

/**
 * @brief Represents a single L3 market-by-order event from Databento NDJSON files.
 *
 * Fields follow the Databento MBO schema. Prices are stored as fixed-precision
 * int64_t (1 unit = 1e-9) and converted to double only for display.
 */
struct MarketDataEvent {
    // ── Timestamps (nanoseconds since UNIX epoch) ─────────────────────────────
    uint64_t ts_recv  = 0;   ///< Databento receive timestamp (index / sort key)
    uint64_t ts_event = 0;   ///< Exchange event timestamp

    // ── Identifiers ───────────────────────────────────────────────────────────
    uint32_t publisher_id   = 0;
    uint32_t instrument_id  = 0;
    uint64_t order_id       = 0;

    // ── Order fields ──────────────────────────────────────────────────────────
    int64_t  price = 0;      ///< Fixed-precision (divide by 1e9 for decimal)
    uint32_t size  = 0;
    char     action = ' ';  ///< A=Add, M=Modify, C=Cancel, R=Clear, T=Trade, F=Fill
    char     side   = ' ';  ///< A=Ask/Sell, B=Bid/Buy, N=None
    uint8_t  flags  = 0;

    // ── Symbology ─────────────────────────────────────────────────────────────
    std::string symbol;      ///< raw_symbol from definition records
    std::string dataset;     ///< e.g. "EUREX.EOBI"

    // ── Helpers ───────────────────────────────────────────────────────────────
    [[nodiscard]] double price_decimal() const noexcept {
        return static_cast<double>(price) * 1e-9;
    }

    [[nodiscard]] bool is_undefined_price() const noexcept {
        // UNDEF_PRICE = INT64_MAX
        return price == std::numeric_limits<int64_t>::max();
    }

    [[nodiscard]] std::string_view action_str() const noexcept {
        switch (action) {
            case 'A': return "Add";
            case 'M': return "Modify";
            case 'C': return "Cancel";
            case 'R': return "Clear";
            case 'T': return "Trade";
            case 'F': return "Fill";
            case 'N': return "None";
            default:  return "Unknown";
        }
    }

    [[nodiscard]] std::string_view side_str() const noexcept {
        switch (side) {
            case 'A': return "Ask";
            case 'B': return "Bid";
            default:  return "None";
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const MarketDataEvent& e) {
        os << "ts_recv="      << e.ts_recv
           << " ts_event="    << e.ts_event
           << " order_id="    << e.order_id
           << " action="      << e.action_str()
           << " side="        << e.side_str()
           << " price="       << e.price_decimal()
           << " size="        << e.size
           << " instrument="  << e.instrument_id
           << " flags=0x"     << std::hex << static_cast<int>(e.flags) << std::dec;
        return os;
    }

    // Comparison by index timestamp (ts_recv first, ts_event as tiebreaker)
    bool operator>(const MarketDataEvent& other) const noexcept {
        if (ts_recv != other.ts_recv) return ts_recv > other.ts_recv;
        return ts_event > other.ts_event;
    }
};
