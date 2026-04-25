#pragma once

#include "BasicTypes.hpp"

namespace cmf {


struct OrderState {
    OrderId orderId = 0;
    std::uint32_t instrumentId = 0;
    Side side = Side::None;
    Price price = 0;
    Quantity qty = 0;
};

} // namespace cmf



