#include "TestSupport.hpp"

#include "backtest/SimulatedOrder.hpp"
#include "backtest/StrategyAction.hpp"

#include <variant>

namespace md::test {

void testSimulatedOrderInitialState() {
    SimulatedOrder order{
        .client_order_id = 17,
        .instrument_id = 442,
        .side = SimOrderSide::Buy,
        .price = 1'156'040'000,
        .quantity = 25,
        .created_timestamp = 1773014580129732099ULL
    };

    require(order.client_order_id == 17, "simulated_order_initial_state: client order id stored");
    require(order.instrument_id == 442, "simulated_order_initial_state: instrument id stored");
    require(order.side == SimOrderSide::Buy, "simulated_order_initial_state: side stored");
    require(order.price == 1'156'040'000, "simulated_order_initial_state: price stored");
    require(order.quantity == 25, "simulated_order_initial_state: quantity stored");
    require(order.remaining_quantity == order.quantity, "simulated_order_initial_state: remaining quantity starts full");
    require(
        order.created_timestamp == 1773014580129732099ULL,
        "simulated_order_initial_state: created timestamp stored"
    );
    require(order.status == SimOrderStatus::New, "simulated_order_initial_state: new order status");
}

void testPlaceOrderActionFields() {
    PlaceOrderAction action{
        .instrument_id = 445,
        .side = SimOrderSide::Sell,
        .price = 1'161'070'000,
        .quantity = 40
    };
    StrategyAction strategy_action = action;

    const auto* placed = std::get_if<PlaceOrderAction>(&strategy_action);
    require(placed != nullptr, "place_order_action_fields: variant stores place action");
    require(placed->instrument_id == 445, "place_order_action_fields: instrument id stored");
    require(placed->side == SimOrderSide::Sell, "place_order_action_fields: side stored");
    require(placed->price == 1'161'070'000, "place_order_action_fields: price stored");
    require(placed->quantity == 40, "place_order_action_fields: quantity stored");
}

void testCancelOrderActionFields() {
    CancelOrderAction action{
        .client_order_id = 17
    };
    StrategyAction strategy_action = action;

    const auto* cancelled = std::get_if<CancelOrderAction>(&strategy_action);
    require(cancelled != nullptr, "cancel_order_action_fields: variant stores cancel action");
    require(cancelled->client_order_id == 17, "cancel_order_action_fields: client order id stored");
}

} // namespace md::test
