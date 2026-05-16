#include "TestSupport.hpp"

#include "processing/BacktestMarketDataEventProcessor.hpp"

#include <string>
#include <utility>
#include <vector>

namespace md::test {
namespace {

MarketDataEvent addEvent(
    std::uint64_t timestamp,
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size,
    std::uint64_t instrument_id = 42
) {
    MarketDataEvent event;
    event.timestamp = timestamp;
    event.ts_recv = timestamp;
    event.ts_event = timestamp;
    event.order_id = order_id;
    event.side = side;
    event.price = price;
    event.size = size;
    event.action = Action::Add;
    event.instrument_id = instrument_id;
    return event;
}

class ScriptedStrategy final : public Strategy {
public:
    explicit ScriptedStrategy(std::vector<std::vector<StrategyAction>> actions_by_call = {})
        : actions_by_call_(std::move(actions_by_call)) {}

    std::string name() const override {
        return "scripted";
    }

    std::vector<StrategyAction> onMarketData(
        const MarketDataEvent& event,
        const MarketView& market,
        const Portfolio& portfolio,
        const OrderManager& orders
    ) override {
        (void)event;
        (void)portfolio;
        seen_markets.push_back(market);
        seen_live_order_counts.push_back(orders.liveOrderCount());

        if (call_count_ < actions_by_call_.size()) {
            return actions_by_call_[call_count_++];
        }

        ++call_count_;
        return {};
    }

    std::vector<MarketView> seen_markets;
    std::vector<std::size_t> seen_live_order_counts;

private:
    std::vector<std::vector<StrategyAction>> actions_by_call_;
    std::size_t call_count_{};
};

PlaceOrderAction placeBuy100(std::uint64_t quantity = 10) {
    return PlaceOrderAction{
        .instrument_id = 42,
        .side = SimOrderSide::Buy,
        .price = 100,
        .quantity = quantity
    };
}

void runBuyCrossingSequence(BacktestMarketDataEventProcessor& processor) {
    processor.processMarketDataEvent(addEvent(100, 1, Side::Bid, 99, 10));
    processor.processMarketDataEvent(addEvent(100, 2, Side::Ask, 101, 10));
    processor.processMarketDataEvent(addEvent(200, 3, Side::Ask, 100, 10));
}

} // namespace

void testBacktestProcessorUpdatesLobBeforeStrategyCall() {
    ScriptedStrategy strategy;
    BacktestMarketDataEventProcessor processor{strategy};

    processor.processMarketDataEvent(addEvent(100, 1, Side::Bid, 99, 10));
    processor.processMarketDataEvent(addEvent(100, 2, Side::Ask, 101, 10));

    require(strategy.seen_markets.size() == 2, "backtest_processor_updates_lob_before_strategy_call: calls");
    require(
        strategy.seen_markets[0].best_bid == 99,
        "backtest_processor_updates_lob_before_strategy_call: first call sees updated bid"
    );
    require(
        strategy.seen_markets[1].best_bid == 99,
        "backtest_processor_updates_lob_before_strategy_call: second call sees bid"
    );
    require(
        strategy.seen_markets[1].best_ask == 101,
        "backtest_processor_updates_lob_before_strategy_call: second call sees updated ask"
    );
}

void testBacktestProcessorFillsExistingOrderAfterMarketCross() {
    ScriptedStrategy strategy{{
        {},
        {placeBuy100()},
        {}
    }};
    BacktestMarketDataEventProcessor processor{strategy};

    runBuyCrossingSequence(processor);

    const auto* order = processor.orders().findOrder(1);
    const auto* position = processor.portfolio().findPosition(42);

    require(order != nullptr, "backtest_processor_fills_existing_order_after_market_cross: order exists");
    require(order->status == SimOrderStatus::Filled, "backtest_processor_fills_existing_order_after_market_cross: filled status");
    require(order->remaining_quantity == 0, "backtest_processor_fills_existing_order_after_market_cross: zero remaining");
    require(processor.orders().liveOrderCount() == 0, "backtest_processor_fills_existing_order_after_market_cross: no live orders");
    require(position != nullptr, "backtest_processor_fills_existing_order_after_market_cross: position exists");
    require(position->inventory == 10, "backtest_processor_fills_existing_order_after_market_cross: inventory");
    require(position->cash == -1000.0L, "backtest_processor_fills_existing_order_after_market_cross: cash");
    require(position->turnover == 1000.0L, "backtest_processor_fills_existing_order_after_market_cross: turnover");
}

void testBacktestProcessorAppliesStrategyPlaceOrderAction() {
    ScriptedStrategy strategy{{
        {placeBuy100(5)}
    }};
    BacktestMarketDataEventProcessor processor{strategy};

    processor.processMarketDataEvent(addEvent(100, 1, Side::Ask, 101, 10));

    const auto* order = processor.orders().findOrder(1);

    require(processor.orders().totalPlaced() == 1, "backtest_processor_applies_strategy_place_order_action: total placed");
    require(processor.orders().liveOrderCount() == 1, "backtest_processor_applies_strategy_place_order_action: live count");
    require(order != nullptr, "backtest_processor_applies_strategy_place_order_action: order exists");
    require(order->instrument_id == 42, "backtest_processor_applies_strategy_place_order_action: instrument");
    require(order->side == SimOrderSide::Buy, "backtest_processor_applies_strategy_place_order_action: side");
    require(order->price == 100, "backtest_processor_applies_strategy_place_order_action: price");
    require(order->quantity == 5, "backtest_processor_applies_strategy_place_order_action: quantity");
    require(order->created_timestamp == 100, "backtest_processor_applies_strategy_place_order_action: timestamp");
}

void testBacktestProcessorAppliesStrategyCancelOrderAction() {
    ScriptedStrategy strategy{{
        {placeBuy100(5)},
        {CancelOrderAction{.client_order_id = 1}}
    }};
    BacktestMarketDataEventProcessor processor{strategy};

    processor.processMarketDataEvent(addEvent(100, 1, Side::Ask, 101, 10));
    processor.processMarketDataEvent(addEvent(200, 2, Side::Ask, 102, 10));

    const auto* order = processor.orders().findOrder(1);

    require(order != nullptr, "backtest_processor_applies_strategy_cancel_order_action: order exists");
    require(order->status == SimOrderStatus::Cancelled, "backtest_processor_applies_strategy_cancel_order_action: cancelled status");
    require(processor.orders().totalCancelled() == 1, "backtest_processor_applies_strategy_cancel_order_action: total cancelled");
    require(processor.orders().liveOrderCount() == 0, "backtest_processor_applies_strategy_cancel_order_action: no live orders");
}

void testBacktestProcessorUpdatesMetrics() {
    ScriptedStrategy strategy{{
        {},
        {placeBuy100()},
        {}
    }};
    BacktestMarketDataEventProcessor processor{strategy};

    runBuyCrossingSequence(processor);

    const auto& metrics = processor.metrics();
    const auto& current = metrics.current();

    require(metrics.observedMarkets() == 3, "backtest_processor_updates_metrics: observed markets");
    require(metrics.observedFills() == 1, "backtest_processor_updates_metrics: observed fills");
    require(metrics.lastTimestamp() == 200, "backtest_processor_updates_metrics: last timestamp");
    require(current.fills == 1, "backtest_processor_updates_metrics: current fills");
    require(current.inventory == 10, "backtest_processor_updates_metrics: current inventory");
    require(current.cash == -1000.0L, "backtest_processor_updates_metrics: current cash");
    require(current.turnover == 1000.0L, "backtest_processor_updates_metrics: current turnover");
}

} // namespace md::test
