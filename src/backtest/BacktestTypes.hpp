#pragma once

namespace md {

enum class SimOrderSide {
    Buy,
    Sell
};

enum class SimOrderStatus {
    New,
    Live,
    Cancelled,
    Filled
};

} // namespace md
