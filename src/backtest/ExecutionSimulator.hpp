#pragma once

#include "backtest/MarketView.hpp"
#include "backtest/SimulatedOrder.hpp"

#include <cstdint>
#include <vector>

namespace md {

struct Fill {
    std::uint64_t client_order_id{};
    std::uint64_t instrument_id{};
    SimOrderSide side{};
    std::int64_t price{};
    std::uint64_t quantity{};
    std::uint64_t timestamp{};
};

class ExecutionSimulator {
public:
    std::vector<Fill> checkFills(
        const MarketView& market,
        std::vector<SimulatedOrder*>& live_orders
    );
};

} // namespace md
