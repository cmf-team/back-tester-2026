#pragma once

#include "backtest/BacktestMetrics.hpp"
#include "backtest/ExecutionSimulator.hpp"
#include "backtest/OrderManager.hpp"
#include "backtest/Portfolio.hpp"
#include "book/BookManager.hpp"
#include "processing/IMarketDataEventProcessor.hpp"
#include "strategies/Strategy.hpp"

#include <cstddef>
#include <cstdint>

namespace md {

class BacktestMarketDataEventProcessor final : public IMarketDataEventProcessor {
public:
    explicit BacktestMarketDataEventProcessor(Strategy& strategy);
    BacktestMarketDataEventProcessor(Strategy& strategy, Portfolio portfolio);

    void processMarketDataEvent(const MarketDataEvent& event) override;

    [[nodiscard]] const BookManager& books() const noexcept;
    [[nodiscard]] const OrderManager& orders() const noexcept;
    [[nodiscard]] const Portfolio& portfolio() const noexcept;
    [[nodiscard]] const BacktestMetricsCollector& metrics() const noexcept;
    [[nodiscard]] std::size_t processedCount() const noexcept;

private:
    void applyStrategyAction(const StrategyAction& action, std::uint64_t timestamp);
    void applyPlaceOrderAction(const PlaceOrderAction& action, std::uint64_t timestamp);
    void applyCancelOrderAction(const CancelOrderAction& action);

    Strategy* strategy_{};
    BookManager book_manager_;
    OrderManager order_manager_;
    ExecutionSimulator execution_simulator_;
    Portfolio portfolio_;
    BacktestMetricsCollector metrics_;
    std::size_t processed_count_{};
};

} // namespace md
