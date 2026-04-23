#pragma once

#include "common/BasicTypes.hpp"
#include "enums.hpp"
#include <optional>



namespace cmf {

struct MarketDataEvent {
    NanoTime ts_received;
    NanoTime ts_event;
    
    Price price;
    Quantity qty;
    
    OrderId order_id;
    SecurityId instrument_id;
    MarketId market_id;
    std::uint16_t channel_id;

    action::Action action;
    side::Side side;
    std::uint8_t flags;

    std::uint32_t sequence;
    std::string symbol;

};

}