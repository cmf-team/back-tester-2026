#include "processing/BacktestMarketDataEventProcessor.hpp"

#include <type_traits>
#include <variant>

namespace md {

BacktestMarketDataEventProcessor::BacktestMarketDataEventProcessor(Strategy& strategy)
    : BacktestMarketDataEventProcessor(strategy, Portfolio{}) {}

BacktestMarketDataEventProcessor::BacktestMarketDataEventProcessor(
    Strategy& strategy,
    Portfolio portfolio
) : strategy_(&strategy),
    portfolio_(portfolio) {}

void BacktestMarketDataEventProcessor::processMarketDataEvent(const MarketDataEvent& event) {
    book_manager_.apply(event);

    auto market = makeMarketView(book_manager_, event.instrument_id, event.timestamp);

    auto live_orders = order_manager_.liveOrdersForInstrument(event.instrument_id);
    auto fills = execution_simulator_.checkFills(market, live_orders);

    for (const auto& fill : fills) {
        portfolio_.applyFill(fill);
        metrics_.observeFill(fill);
    }

    auto actions = strategy_->onMarketData(event, market, portfolio_, order_manager_);
    for (const auto& action : actions) {
        applyStrategyAction(action, event.timestamp);
    }

    metrics_.observeMarket(event, market, portfolio_);
    ++processed_count_;
}

const BookManager& BacktestMarketDataEventProcessor::books() const noexcept {
    return book_manager_;
}

const OrderManager& BacktestMarketDataEventProcessor::orders() const noexcept {
    return order_manager_;
}

const Portfolio& BacktestMarketDataEventProcessor::portfolio() const noexcept {
    return portfolio_;
}

const BacktestMetricsCollector& BacktestMarketDataEventProcessor::metrics() const noexcept {
    return metrics_;
}

std::size_t BacktestMarketDataEventProcessor::processedCount() const noexcept {
    return processed_count_;
}

void BacktestMarketDataEventProcessor::applyStrategyAction(
    const StrategyAction& action,
    std::uint64_t timestamp
) {
    std::visit(
        [this, timestamp](const auto& concrete_action) {
            if constexpr (std::is_same_v<std::decay_t<decltype(concrete_action)>, PlaceOrderAction>) {
                applyPlaceOrderAction(concrete_action, timestamp);
            } else {
                applyCancelOrderAction(concrete_action);
            }
        },
        action
    );
}

void BacktestMarketDataEventProcessor::applyPlaceOrderAction(
    const PlaceOrderAction& action,
    std::uint64_t timestamp
) {
    order_manager_.placeLimitOrder(
        action.instrument_id,
        action.side,
        action.price,
        action.quantity,
        timestamp
    );
}

void BacktestMarketDataEventProcessor::applyCancelOrderAction(const CancelOrderAction& action) {
    order_manager_.cancelOrder(action.client_order_id);
}

} // namespace md
