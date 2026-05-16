#pragma once

#include "backtest/ExecutionSimulator.hpp"
#include "backtest/MarketView.hpp"
#include "backtest/Portfolio.hpp"
#include "domain/MarketDataEvent.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace md {

struct InstrumentMetrics {
    std::uint64_t instrument_id{};
    std::int64_t inventory{};
    long double cash{};
    long double turnover{};
    std::uint64_t fills{};
    std::optional<long double> mtm_pnl;
};

struct BacktestMetrics {
    long double cash{};
    long double turnover{};
    long double mtm_pnl{};
    std::int64_t inventory{};
    std::uint64_t fills{};
    std::vector<InstrumentMetrics> instruments;
};

[[nodiscard]] long double markToMarketPnl(
    const InstrumentPosition& position,
    std::int64_t mid_price,
    long double multiplier
);

[[nodiscard]] BacktestMetrics makeBacktestMetrics(
    const Portfolio& portfolio,
    const std::vector<MarketView>& market_views
);

class BacktestMetricsCollector {
public:
    void observeFill(const Fill& fill);
    void observeMarket(
        const MarketDataEvent& event,
        const MarketView& market,
        const Portfolio& portfolio
    );

    [[nodiscard]] const BacktestMetrics& current() const noexcept;
    [[nodiscard]] std::uint64_t observedFills() const noexcept;
    [[nodiscard]] std::uint64_t observedMarkets() const noexcept;
    [[nodiscard]] std::uint64_t lastTimestamp() const noexcept;
    [[nodiscard]] std::int64_t maxInventory() const noexcept;
    [[nodiscard]] long double averageInventory() const noexcept;

private:
    BacktestMetrics current_;
    std::uint64_t observed_fills_{};
    std::uint64_t observed_markets_{};
    std::uint64_t last_timestamp_{};
    std::int64_t max_inventory_{};
    long double inventory_sample_sum_{};
};

} // namespace md
