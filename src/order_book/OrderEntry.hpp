#pragma once

#include "common/BasicTypes.hpp"
#include <cstdint>

namespace cmf
{

struct PriceLevel;

struct OrderEntry
{
    OrderId order_id = 0;
    Side side = Side::None;
    int64_t quantity = 0;
    ScaledPrice price = 0;
    NanoTime entry_time = 0;
    NanoTime last_update = 0;

    OrderEntry* next = nullptr;
    OrderEntry* prev = nullptr;
    PriceLevel* level = nullptr;
};

} // namespace cmf
