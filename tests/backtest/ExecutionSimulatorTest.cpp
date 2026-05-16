#include "TestSupport.hpp"

#include "backtest/ExecutionSimulator.hpp"

#include <vector>

namespace md::test {
namespace {

SimulatedOrder liveOrder(
    std::uint64_t client_order_id,
    SimOrderSide side,
    std::int64_t price,
    std::uint64_t quantity
) {
    SimulatedOrder order;
    order.client_order_id = client_order_id;
    order.instrument_id = 42;
    order.side = side;
    order.price = price;
    order.quantity = quantity;
    order.remaining_quantity = quantity;
    order.created_timestamp = 1;
    order.status = SimOrderStatus::Live;
    return order;
}

MarketView market(std::optional<std::int64_t> best_bid, std::optional<std::int64_t> best_ask) {
    MarketView view;
    view.instrument_id = 42;
    view.timestamp = 12345;
    view.best_bid = best_bid;
    view.best_ask = best_ask;
    return view;
}

std::vector<Fill> checkSingleOrder(ExecutionSimulator& simulator, const MarketView& view, SimulatedOrder& order) {
    std::vector<SimulatedOrder*> live_orders{&order};
    return simulator.checkFills(view, live_orders);
}

} // namespace

void testBuyOrderFillsWhenBestAskCrossesLimitPrice() {
    ExecutionSimulator simulator;
    auto order = liveOrder(17, SimOrderSide::Buy, 100, 10);

    const auto fills = checkSingleOrder(simulator, market(std::nullopt, 99), order);

    require(fills.size() == 1, "buy_order_fills_when_best_ask_crosses_limit_price: one fill");
    require(fills.front().client_order_id == 17, "buy_order_fills_when_best_ask_crosses_limit_price: fill order id");
    require(fills.front().instrument_id == 42, "buy_order_fills_when_best_ask_crosses_limit_price: fill instrument");
    require(fills.front().side == SimOrderSide::Buy, "buy_order_fills_when_best_ask_crosses_limit_price: fill side");
    require(fills.front().price == 100, "buy_order_fills_when_best_ask_crosses_limit_price: fill at limit price");
    require(fills.front().quantity == 10, "buy_order_fills_when_best_ask_crosses_limit_price: full quantity");
    require(fills.front().timestamp == 12345, "buy_order_fills_when_best_ask_crosses_limit_price: fill timestamp");
}

void testBuyOrderDoesNotFillWhenBestAskAboveLimitPrice() {
    ExecutionSimulator simulator;
    auto order = liveOrder(17, SimOrderSide::Buy, 100, 10);

    const auto fills = checkSingleOrder(simulator, market(std::nullopt, 101), order);

    require(fills.empty(), "buy_order_does_not_fill_when_best_ask_above_limit_price: no fill");
    require(order.status == SimOrderStatus::Live, "buy_order_does_not_fill_when_best_ask_above_limit_price: still live");
    require(order.remaining_quantity == 10, "buy_order_does_not_fill_when_best_ask_above_limit_price: quantity unchanged");
}

void testSellOrderFillsWhenBestBidCrossesLimitPrice() {
    ExecutionSimulator simulator;
    auto order = liveOrder(18, SimOrderSide::Sell, 105, 7);

    const auto fills = checkSingleOrder(simulator, market(106, std::nullopt), order);

    require(fills.size() == 1, "sell_order_fills_when_best_bid_crosses_limit_price: one fill");
    require(fills.front().client_order_id == 18, "sell_order_fills_when_best_bid_crosses_limit_price: fill order id");
    require(fills.front().instrument_id == 42, "sell_order_fills_when_best_bid_crosses_limit_price: fill instrument");
    require(fills.front().side == SimOrderSide::Sell, "sell_order_fills_when_best_bid_crosses_limit_price: fill side");
    require(fills.front().price == 105, "sell_order_fills_when_best_bid_crosses_limit_price: fill at limit price");
    require(fills.front().quantity == 7, "sell_order_fills_when_best_bid_crosses_limit_price: full quantity");
    require(fills.front().timestamp == 12345, "sell_order_fills_when_best_bid_crosses_limit_price: fill timestamp");
}

void testSellOrderDoesNotFillWhenBestBidBelowLimitPrice() {
    ExecutionSimulator simulator;
    auto order = liveOrder(18, SimOrderSide::Sell, 105, 7);

    const auto fills = checkSingleOrder(simulator, market(104, std::nullopt), order);

    require(fills.empty(), "sell_order_does_not_fill_when_best_bid_below_limit_price: no fill");
    require(order.status == SimOrderStatus::Live, "sell_order_does_not_fill_when_best_bid_below_limit_price: still live");
    require(order.remaining_quantity == 7, "sell_order_does_not_fill_when_best_bid_below_limit_price: quantity unchanged");
}

void testFilledOrderRemainingQuantityBecomesZero() {
    ExecutionSimulator simulator;
    auto order = liveOrder(17, SimOrderSide::Buy, 100, 10);

    checkSingleOrder(simulator, market(std::nullopt, 99), order);

    require(order.remaining_quantity == 0, "filled_order_remaining_quantity_becomes_zero: zero remaining");
}

void testFilledOrderStatusBecomesFilled() {
    ExecutionSimulator simulator;
    auto order = liveOrder(18, SimOrderSide::Sell, 105, 7);

    checkSingleOrder(simulator, market(106, std::nullopt), order);

    require(order.status == SimOrderStatus::Filled, "filled_order_status_becomes_filled: filled status");
}

} // namespace md::test
