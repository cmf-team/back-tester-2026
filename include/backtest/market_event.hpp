#pragma once
#include <cstdint>

namespace backtest {
enum Side : uint8_t
{
    None = 0, 
    Buy = 1, 
    Sell = 2 
};

enum EventType : uint8_t
{
    Trade,
};

struct alignas(16) MarketEvent
{
    int64_t   timestamp_us;  //  8   offset  0
    int64_t   price_ticks;   //  8   offset  8 // цена × множитель тика (например, 150.25 → 15025 при тике 0.01)
    int32_t   amount;        //  4   offset 16
    EventType type;          //  1   offset 20
    Side      side;          //  1   offset 21
    uint8_t   pad_[10];      // 10   offset 22 // NOLINT
};
}