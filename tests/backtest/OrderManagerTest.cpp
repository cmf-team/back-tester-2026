#include "TestSupport.hpp"

#include "backtest/OrderManager.hpp"

namespace md::test {

void testOrderManagerPlacesBuyLimitOrder() {
    OrderManager manager;

    const auto order_id = manager.placeLimitOrder(1, SimOrderSide::Buy, 100, 10, 12345);
    const auto* order = manager.findOrder(order_id);

    require(order != nullptr, "order_manager_places_buy_limit_order: order is stored");
    require(order->client_order_id == order_id, "order_manager_places_buy_limit_order: client order id stored");
    require(order->instrument_id == 1, "order_manager_places_buy_limit_order: instrument id stored");
    require(order->side == SimOrderSide::Buy, "order_manager_places_buy_limit_order: buy side stored");
    require(order->price == 100, "order_manager_places_buy_limit_order: price stored");
    require(order->quantity == 10, "order_manager_places_buy_limit_order: quantity stored");
    require(order->remaining_quantity == 10, "order_manager_places_buy_limit_order: remaining quantity starts full");
    require(order->created_timestamp == 12345, "order_manager_places_buy_limit_order: timestamp stored");
    require(order->status == SimOrderStatus::Live, "order_manager_places_buy_limit_order: order becomes live");
    require(manager.totalPlaced() == 1, "order_manager_places_buy_limit_order: total placed");
    require(manager.totalCancelled() == 0, "order_manager_places_buy_limit_order: total cancelled");
    require(manager.liveOrderCount() == 1, "order_manager_places_buy_limit_order: live order count");
}

void testOrderManagerPlacesSellLimitOrder() {
    OrderManager manager;

    const auto order_id = manager.placeLimitOrder(1, SimOrderSide::Sell, 105, 7, 12346);
    const auto* order = manager.findOrder(order_id);

    require(order != nullptr, "order_manager_places_sell_limit_order: order is stored");
    require(order->instrument_id == 1, "order_manager_places_sell_limit_order: instrument id stored");
    require(order->side == SimOrderSide::Sell, "order_manager_places_sell_limit_order: sell side stored");
    require(order->price == 105, "order_manager_places_sell_limit_order: price stored");
    require(order->quantity == 7, "order_manager_places_sell_limit_order: quantity stored");
    require(order->remaining_quantity == 7, "order_manager_places_sell_limit_order: remaining quantity starts full");
    require(order->created_timestamp == 12346, "order_manager_places_sell_limit_order: timestamp stored");
    require(order->status == SimOrderStatus::Live, "order_manager_places_sell_limit_order: order becomes live");
}

void testOrderManagerGeneratesUniqueClientOrderIds() {
    OrderManager manager;

    const auto first = manager.placeLimitOrder(1, SimOrderSide::Buy, 100, 10, 1);
    const auto second = manager.placeLimitOrder(1, SimOrderSide::Sell, 105, 7, 2);
    const auto third = manager.placeLimitOrder(2, SimOrderSide::Buy, 200, 3, 3);

    require(first != second, "order_manager_generates_unique_client_order_ids: first and second differ");
    require(first != third, "order_manager_generates_unique_client_order_ids: first and third differ");
    require(second != third, "order_manager_generates_unique_client_order_ids: second and third differ");
    require(manager.totalPlaced() == 3, "order_manager_generates_unique_client_order_ids: total placed");
    require(manager.liveOrderCount() == 3, "order_manager_generates_unique_client_order_ids: all orders live");
}

void testOrderManagerCancelsLiveOrder() {
    OrderManager manager;

    const auto first = manager.placeLimitOrder(1, SimOrderSide::Buy, 100, 10, 1);
    manager.placeLimitOrder(1, SimOrderSide::Sell, 105, 7, 2);
    manager.placeLimitOrder(2, SimOrderSide::Buy, 200, 3, 3);

    require(manager.cancelOrder(first), "order_manager_cancels_live_order: cancel succeeds");

    const auto* cancelled = manager.findOrder(first);
    require(cancelled != nullptr, "order_manager_cancels_live_order: cancelled order remains inspectable");
    require(cancelled->status == SimOrderStatus::Cancelled, "order_manager_cancels_live_order: cancelled status");
    require(manager.totalPlaced() == 3, "order_manager_cancels_live_order: total placed");
    require(manager.totalCancelled() == 1, "order_manager_cancels_live_order: total cancelled");
    require(manager.liveOrderCount() == 2, "order_manager_cancels_live_order: live order count");
}

void testOrderManagerCancelUnknownOrderReturnsFalse() {
    OrderManager manager;

    require(!manager.cancelOrder(999), "order_manager_cancel_unknown_order_returns_false: unknown cancel fails");
    require(manager.totalPlaced() == 0, "order_manager_cancel_unknown_order_returns_false: total placed unchanged");
    require(manager.totalCancelled() == 0, "order_manager_cancel_unknown_order_returns_false: total cancelled unchanged");
    require(manager.liveOrderCount() == 0, "order_manager_cancel_unknown_order_returns_false: live count unchanged");
}

void testOrderManagerLiveOrdersForInstrumentFiltersCorrectly() {
    OrderManager manager;

    const auto first = manager.placeLimitOrder(1, SimOrderSide::Buy, 100, 10, 1);
    const auto second = manager.placeLimitOrder(1, SimOrderSide::Sell, 105, 7, 2);
    const auto third = manager.placeLimitOrder(2, SimOrderSide::Buy, 200, 3, 3);
    manager.cancelOrder(first);

    const auto instrument_one_orders = manager.liveOrdersForInstrument(1);
    const auto instrument_two_orders = manager.liveOrdersForInstrument(2);

    require(manager.totalPlaced() == 3, "order_manager_live_orders_for_instrument_filters_correctly: total placed");
    require(manager.totalCancelled() == 1, "order_manager_live_orders_for_instrument_filters_correctly: total cancelled");
    require(manager.liveOrderCount() == 2, "order_manager_live_orders_for_instrument_filters_correctly: live count");
    require(instrument_one_orders.size() == 1, "order_manager_live_orders_for_instrument_filters_correctly: instrument 1 count");
    require(instrument_two_orders.size() == 1, "order_manager_live_orders_for_instrument_filters_correctly: instrument 2 count");
    require(
        instrument_one_orders.front()->client_order_id == second,
        "order_manager_live_orders_for_instrument_filters_correctly: instrument 1 live order"
    );
    require(
        instrument_two_orders.front()->client_order_id == third,
        "order_manager_live_orders_for_instrument_filters_correctly: instrument 2 live order"
    );

    const auto* cancelled = manager.findOrder(first);
    require(cancelled != nullptr, "order_manager_live_orders_for_instrument_filters_correctly: cancelled order found");
    require(
        cancelled->status == SimOrderStatus::Cancelled,
        "order_manager_live_orders_for_instrument_filters_correctly: cancelled status"
    );
}

} // namespace md::test
