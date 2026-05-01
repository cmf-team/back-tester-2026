#pragma once
#include <cstdint>

enum class Side : uint8_t
{
    Bid,
    Ask
};

struct Order
{
    uint64_t order_id{};
    uint64_t instrument_id{};
    Side side{};
    int64_t price{}; // scaled 1e9
    int64_t qty{};
};
