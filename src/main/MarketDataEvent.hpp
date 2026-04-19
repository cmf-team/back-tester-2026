#pragma once

#include "common/BasicTypes.hpp"

#include <cmath>
#include <cstdint>

namespace cmf{



struct MarketDataEvent {
    NanoTime ts_recv     = 0;
    NanoTime ts_event    = 0;
    int32_t  ts_in_delta = 0;

    uint16_t publisher_id  = 0;
    uint32_t instrument_id = 0;
    OrderId  order_id      = 0;

    Price    price  = std::numeric_limits<double>::quiet_NaN();
    Quantity size   = 0;

    Action   action = Action::None;
    Side     side   = Side::None;

    uint8_t  flags    = 0;
    uint32_t sequence = 0;
    uint8_t  rtype    = 0;
};
} // namespace cmf
