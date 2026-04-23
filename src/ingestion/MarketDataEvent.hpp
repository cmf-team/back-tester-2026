#pragma once

#include "common/BasicTypes.hpp"
#include <string>

namespace cmf {

enum class Action {
    Add,
    Modify,
    Delete,
    Trade,
    Unknown
};

struct MarketDataEvent {
    NanoTime timestamp{0};
    OrderId orderId{0};
    Side side{Side::None};
    Price price{0.0};
    Quantity size{0.0};
    Action action{Action::Unknown};
};

} // namespace cmf