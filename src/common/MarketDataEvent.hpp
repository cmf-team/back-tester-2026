#pragma once

#include <cstdint>
#include <limits>
#include <string>

enum class Side : std::uint8_t
{
    Bid,
    Ask,
    None
};

enum class Action : std::uint8_t
{
    Add,    // Insert a new order into the book.
    Modify, // Change an order's price and/or size.
    Cancel, // Fully or partially cancel an order from the book.
    Clear,  // Remove all resting orders for the instrument.
    Trade,  // An aggressing order traded. Does not affect the book.
    Fill,   // A resting order was filled. Does not affect the book.
    None    // No action: does not affect the book, but may carry flags or other
            // information.
};

enum class RType : uint8_t
{
    MBP_0 = 0x00,         // Market-by-price record with a book depth of 0. Used for the
                          // trades schema.
    MBP_1 = 0x01,         // Market-by-price record with a book depth of 1. Used for the
                          // TBBO and MBP-1 schemas.
    MBP_10 = 0x0A,        // Market-by-price record with a book depth of 10.
    Status = 0x12,        // Exchange status record.
    Definition = 0x13,    // Instrument definition record.
    Imbalance = 0x14,     // Order imbalance record
    Error = 0x15,         // Error record from the live gateway
    SymbolMapping = 0x16, // Symbol mapping record from the live gateway
    System = 0x17,        // Non-error system record from the live gateway
    Statistics = 0x18,    // Statistics record from the publisher
    OHLCV_1s = 0x20,      // OHLCV record at 1-second cadence
    OHLCV_1m = 0x21,      // OHLCV record at 1-minute cadence
    OHLCV_1h = 0x22,      // OHLCV record at hourly cadence
    OHLCV_1d = 0x23,      // OHLCV record at daily cadence
    MBO = 0xA0,           // Market-by-order record
    CMBP_1 = 0xB1,        // Consolidated MBP record with a book depth of 1
    CBBO_1s = 0xC0,       // CMBP-1 at a 1-sec cadence.
    CBBO_1m = 0xC1,       // CMBP-1  at a 1-minute cadence.
    TCBBO = 0xC2,         // Consolidated MBP record with a book depth of 1 with only
                          // trades.
    BBO_1s = 0xC3,        // MBP record with a book depth of 1 at a 1-second cadence.
    BBO_1m = 0xC4,        // MBP record with a book depth of 1 at a 1-minute cadence.
};

enum class Flag : std::uint8_t
{
    None = 0,
    F_RESERVED =
        1u
        << 0,                       // Reserved for internal use; can be ignored. May be set or unset.
    F_PUBLISHER_SPECIFIC = 1u << 1, // Semantics depend on the publisher_id.
                                    // Refer to the dataset supplement.
    F_MAYBE_BAD_BOOK =
        1u << 2,             // An unrecoverable gap was detected in the channel.
    F_BAD_TS_RECV = 1u << 3, // The ts_recv value is inaccurate due to clock
                             // issues or packet reordering.
    F_MBP = 1u << 4,         // Aggregated price level message, not an individual order.
    F_SNAPSHOT =
        1u << 5,     // Message sourced from a replay, such as a snapshot server.
    F_TOB = 1u << 6, // Top-of-book message, not an individual order.
    F_LAST = 1u << 7 // Marks the last record in a single event for a given
                     // instrument_id.
};

/*
"ts_recv":"2026-03-09T07:52:41.368148840Z",
"hd":{
  "ts_event":"2026-03-09T07:52:41.367824437Z",
  "rtype":160,
  "publisher_id":101,
  "instrument_id":34513},
"action":"A",
"side":"A",
"price":"0.021630000",
"size":20,
"channel_id":79,
"order_id":"1773042761367855297",
"flags":128,
"ts_in_delta":2365,
"sequence":52012,
"symbol":"EUCO SI 20260710 PS EU P 1.1650 0"}
*/

struct MarketDataEvent
{
    static constexpr std::int64_t UNDEF_PRICE =
        std::numeric_limits<std::int64_t>::max();
    static constexpr std::uint64_t UNDEF_TIMESTAMP =
        std::numeric_limits<std::uint64_t>::max();

    [[nodiscard]] static const std::string sideToString(Side s) noexcept;
    [[nodiscard]] static const std::string actionToString(Action a) noexcept;
    [[nodiscard]] static const std::string flagToString(Flag f) noexcept;
    [[nodiscard]] static const std::string rTypeToString(RType r) noexcept;
    [[nodiscard]] static double priceToDouble(std::int64_t price) noexcept
    {
        return static_cast<double>(price) * 1e-9;
    }

    std::uint64_t sort_ts{UNDEF_TIMESTAMP};
    std::uint64_t ts_recv{UNDEF_TIMESTAMP};
    std::uint64_t ts_event{UNDEF_TIMESTAMP};
    RType rtype{RType::MBP_0};
    std::uint16_t publisher_id{0};
    std::uint32_t instrument_id{0};
    Action action{Action::None};
    Side side{Side::None};
    std::int64_t price{UNDEF_PRICE};
    std::uint32_t size{0};
    std::uint8_t channel_id{0};
    std::uint64_t order_id{0};
    Flag flag{Flag::None};
    std::int32_t ts_in_delta{0};

    std::uint32_t source_file_id{0};
};