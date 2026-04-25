#pragma once

#include "common/BasicTypes.hpp"
#include "common/enums.hpp"
#include <cstdint>

namespace cmf {

struct PriceLevel;

struct OrderEntry {
    OrderId    order_id;
    side::Side side;
    int64_t    quantity;    // resting qty
    int64_t    price;       // scaled: double * 1e9
    NanoTime   entry_time;
    NanoTime   last_update;

    OrderEntry* next  = nullptr;  // newer order (towards tail)
    OrderEntry* prev  = nullptr;  // older order (towards head)
    PriceLevel* level = nullptr;  // owning price level
};

} // namespace cmf
