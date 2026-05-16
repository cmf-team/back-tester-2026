#pragma once

#include "backtest/BacktestTypes.hpp"

#include <cstdint>
#include <variant>

namespace md {

struct PlaceOrderAction {
    std::uint64_t instrument_id{};
    SimOrderSide side{};
    std::int64_t price{};
    std::uint64_t quantity{};
};

struct CancelOrderAction {
    std::uint64_t client_order_id{};
};

using StrategyAction = std::variant<PlaceOrderAction, CancelOrderAction>;

} // namespace md
