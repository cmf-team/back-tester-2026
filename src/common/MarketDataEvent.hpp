#pragma once

#include <cstdint>
#include <limits>
#include <ostream>
#include <string>

enum class Side : std::uint8_t
{
    Bid = 'B',
    Ask = 'A',
    None = 'N'
};

enum class Action : std::uint8_t
{
    Add = 'A',
    Modify = 'M',
    Cancel = 'C',
    Clear = 'R',
    Trade = 'T',
    Fill = 'F',
    None = 'N'
};

enum class RType : uint8_t
{
    Mbp0 = 0x00,
    Mbp1 = 0x01,
    Mbp10 = 0x0A,
    Status = 0x12,
    Definition = 0x13,
    Imbalance = 0x14,
    Error = 0x15,
    SymbolMapping = 0x16,
    System = 0x17,
    Statistics = 0x18,
    Ohlcv1s = 0x20,
    Ohlcv1m = 0x21,
    Ohlcv1h = 0x22,
    Ohlcv1d = 0x23,
    Mbo = 0xA0,
    Cmbp1 = 0xB1,
    Cbbo1s = 0xC0,
    Cbbo1m = 0xC1,
    Tcbbo = 0xC2,
    Bbo1s = 0xC3,
    Bbo1m = 0xC4,
};

enum class Flag : std::uint8_t
{
    None = 0,
    Reserved = 1u << 0,
    PublisherSpecific = 1u << 1,
    MaybeBadBook = 1u << 2,
    BadTsRecv = 1u << 3,
    Mbp = 1u << 4,
    Snapshot = 1u << 5,
    Tob = 1u << 6,
    Last = 1u << 7
};

struct MarketDataEvent
{
    static constexpr std::int64_t UNDEF_PRICE =
        std::numeric_limits<std::int64_t>::max();
    static constexpr std::uint64_t UNDEF_TIMESTAMP =
        std::numeric_limits<std::uint64_t>::max();

    [[nodiscard]] static auto priceToDouble(std::int64_t price) noexcept -> double
    {
        return static_cast<double>(price) * 1e-9;
    }

    std::uint64_t ts_recv{UNDEF_TIMESTAMP};
    std::uint64_t ts_event{UNDEF_TIMESTAMP};
    RType rtype{RType::Mbp0};
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

    friend auto operator<<(std::ostream& os, const MarketDataEvent& e) -> std::ostream&;
};