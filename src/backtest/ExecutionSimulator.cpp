#include "backtest/ExecutionSimulator.hpp"

namespace md {
namespace {

bool crossesBuyLimit(const SimulatedOrder& order, const MarketView& market) {
    return market.best_ask.has_value() && *market.best_ask <= order.price;
}

bool crossesSellLimit(const SimulatedOrder& order, const MarketView& market) {
    return market.best_bid.has_value() && *market.best_bid >= order.price;
}

bool shouldFill(const SimulatedOrder& order, const MarketView& market) {
    if (order.instrument_id != market.instrument_id
        || order.status != SimOrderStatus::Live
        || order.remaining_quantity == 0) {
        return false;
    }

    switch (order.side) {
        case SimOrderSide::Buy:
            return crossesBuyLimit(order, market);
        case SimOrderSide::Sell:
            return crossesSellLimit(order, market);
    }

    return false;
}

Fill makeFill(const SimulatedOrder& order, const MarketView& market) {
    return Fill{
        .client_order_id = order.client_order_id,
        .instrument_id = order.instrument_id,
        .side = order.side,
        .price = order.price,
        .quantity = order.remaining_quantity,
        .timestamp = market.timestamp
    };
}

} // namespace

std::vector<Fill> ExecutionSimulator::checkFills(
    const MarketView& market,
    std::vector<SimulatedOrder*>& live_orders
) {
    std::vector<Fill> fills;

    for (auto* order : live_orders) {
        if (order == nullptr || !shouldFill(*order, market)) {
            continue;
        }

        fills.push_back(makeFill(*order, market));
        order->remaining_quantity = 0;
        order->status = SimOrderStatus::Filled;
    }

    return fills;
}

} // namespace md
