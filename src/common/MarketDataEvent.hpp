#pragma once

#include <cstdint>
#include "BasicTypes.hpp"

namespace cmf {

using RType = std::uint8_t;
using Flags = std::uint8_t;
using Action = char;


struct MarketDataEvent {
    NanoTime ts_recv = 0;
    NanoTime ts_event = 0;

    RType rtype = 0;
    std::uint32_t publisher_id = 0;
    std::uint32_t instrument_id = 0;

    Action action = 'N';
    Side side = Side::None;

    Price price = 0;           // scaled integer with PriceScale
    std::uint32_t size = 0;

    std::uint16_t channel_id = 0;
    OrderId order_id = 0;

    Flags flags = 0;
    std::int32_t ts_in_delta = 0;
    std::uint32_t sequence = 0;
};

} // namespace cmf