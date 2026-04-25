#pragma once

#include "BasicTypes.hpp"

namespace cmf
{

struct MarketDataEvent
{
    NanoTime timestamp;
    uint32_t instrument_id;
    OrderId order_id;
    Side side;
    Price price;
    Quantity size;
    Action action;
};

} // namespace cmf