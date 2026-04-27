#pragma once

#include "common/BasicTypes.hpp"

using InstrumentId = std::int64_t;

enum class OrderAction : signed char {
    None = 0,
    Add = 1,
    Modify = 2,
    Cancel = 3,
    Trade = 4,
    Fill = 5,
    Clear = 6
};

struct MarketDataEvent {
    cmf::NanoTime ts_event{};
    cmf::OrderId order_id{};
    InstrumentId instrument_id{};
    cmf::Side side{};
    cmf::Price price{};
    cmf::Quantity size{};
    OrderAction action{OrderAction::None};
};