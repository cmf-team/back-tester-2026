#pragma once

#include "backtest/BacktestTypes.hpp"

#include <cstdint>

namespace md {

struct SimulatedOrder {
    std::uint64_t client_order_id{};
    std::uint64_t instrument_id{};
    SimOrderSide side{};
    std::int64_t price{};
    std::uint64_t quantity{};
    std::uint64_t remaining_quantity{quantity};
    std::uint64_t created_timestamp{};
    SimOrderStatus status{SimOrderStatus::New};
};

} // namespace md
