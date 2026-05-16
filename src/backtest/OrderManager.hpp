#pragma once

#include "backtest/SimulatedOrder.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace md {

class OrderManager {
public:
    std::uint64_t placeLimitOrder(
        std::uint64_t instrument_id,
        SimOrderSide side,
        std::int64_t price,
        std::uint64_t quantity,
        std::uint64_t timestamp
    );

    bool cancelOrder(std::uint64_t client_order_id);

    std::vector<SimulatedOrder*> liveOrdersForInstrument(std::uint64_t instrument_id);
    std::vector<const SimulatedOrder*> liveOrdersForInstrument(std::uint64_t instrument_id) const;
    const SimulatedOrder* findOrder(std::uint64_t client_order_id) const;

    std::size_t liveOrderCount() const noexcept;
    std::size_t totalPlaced() const noexcept;
    std::size_t totalCancelled() const noexcept;

private:
    std::uint64_t next_client_order_id_{1};
    std::unordered_map<std::uint64_t, SimulatedOrder> orders_;
    std::size_t total_placed_{};
    std::size_t total_cancelled_{};
};

} // namespace md
