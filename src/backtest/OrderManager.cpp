#include "backtest/OrderManager.hpp"

namespace md {

std::uint64_t OrderManager::placeLimitOrder(
    std::uint64_t instrument_id,
    SimOrderSide side,
    std::int64_t price,
    std::uint64_t quantity,
    std::uint64_t timestamp
) {
    const auto client_order_id = next_client_order_id_++;

    SimulatedOrder order;
    order.client_order_id = client_order_id;
    order.instrument_id = instrument_id;
    order.side = side;
    order.price = price;
    order.quantity = quantity;
    order.remaining_quantity = quantity;
    order.created_timestamp = timestamp;
    order.status = SimOrderStatus::Live;

    orders_.emplace(client_order_id, order);
    ++total_placed_;

    return client_order_id;
}

bool OrderManager::cancelOrder(std::uint64_t client_order_id) {
    auto iter = orders_.find(client_order_id);
    if (iter == orders_.end() || iter->second.status != SimOrderStatus::Live) {
        return false;
    }

    iter->second.status = SimOrderStatus::Cancelled;
    ++total_cancelled_;
    return true;
}

std::vector<SimulatedOrder*> OrderManager::liveOrdersForInstrument(std::uint64_t instrument_id) {
    std::vector<SimulatedOrder*> live_orders;
    for (auto& [_, order] : orders_) {
        if (order.instrument_id == instrument_id && order.status == SimOrderStatus::Live) {
            live_orders.push_back(&order);
        }
    }
    return live_orders;
}

std::vector<const SimulatedOrder*> OrderManager::liveOrdersForInstrument(std::uint64_t instrument_id) const {
    std::vector<const SimulatedOrder*> live_orders;
    for (const auto& [_, order] : orders_) {
        if (order.instrument_id == instrument_id && order.status == SimOrderStatus::Live) {
            live_orders.push_back(&order);
        }
    }
    return live_orders;
}

const SimulatedOrder* OrderManager::findOrder(std::uint64_t client_order_id) const {
    auto iter = orders_.find(client_order_id);
    if (iter == orders_.end()) {
        return nullptr;
    }
    return &iter->second;
}

std::size_t OrderManager::liveOrderCount() const noexcept {
    std::size_t live_count = 0;
    for (const auto& [_, order] : orders_) {
        if (order.status == SimOrderStatus::Live) {
            ++live_count;
        }
    }
    return live_count;
}

std::size_t OrderManager::totalPlaced() const noexcept {
    return total_placed_;
}

std::size_t OrderManager::totalCancelled() const noexcept {
    return total_cancelled_;
}

} // namespace md
