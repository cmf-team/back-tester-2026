#include "backtest/BacktestMetrics.hpp"

namespace md {
namespace {

std::optional<std::int64_t> findMidPrice(
    const std::vector<MarketView>& market_views,
    std::uint64_t instrument_id
) {
    for (const auto& market : market_views) {
        if (market.instrument_id == instrument_id) {
            return market.mid_price;
        }
    }
    return std::nullopt;
}

std::int64_t absoluteInventory(std::int64_t inventory) {
    return inventory < 0 ? -inventory : inventory;
}

} // namespace

long double markToMarketPnl(
    const InstrumentPosition& position,
    std::int64_t mid_price,
    long double multiplier
) {
    return position.cash
        + static_cast<long double>(position.inventory) * static_cast<long double>(mid_price) * multiplier;
}

BacktestMetrics makeBacktestMetrics(
    const Portfolio& portfolio,
    const std::vector<MarketView>& market_views
) {
    BacktestMetrics metrics;

    for (const auto& [instrument_id, position] : portfolio.positions()) {
        InstrumentMetrics instrument;
        instrument.instrument_id = instrument_id;
        instrument.inventory = position.inventory;
        instrument.cash = position.cash;
        instrument.turnover = position.turnover;
        instrument.fills = position.fills;

        if (const auto mid_price = findMidPrice(market_views, instrument_id); mid_price.has_value()) {
            instrument.mtm_pnl = markToMarketPnl(position, *mid_price, portfolio.multiplier());
            metrics.mtm_pnl += *instrument.mtm_pnl;
        }

        metrics.cash += position.cash;
        metrics.turnover += position.turnover;
        metrics.inventory += position.inventory;
        metrics.fills += position.fills;
        metrics.instruments.push_back(instrument);
    }

    return metrics;
}

void BacktestMetricsCollector::observeFill(const Fill& fill) {
    (void)fill;
    ++observed_fills_;
}

void BacktestMetricsCollector::observeMarket(
    const MarketDataEvent& event,
    const MarketView& market,
    const Portfolio& portfolio
) {
    current_ = makeBacktestMetrics(portfolio, std::vector<MarketView>{market});
    ++observed_markets_;
    last_timestamp_ = event.timestamp;

    const auto absolute_inventory = absoluteInventory(current_.inventory);
    if (absolute_inventory > max_inventory_) {
        max_inventory_ = absolute_inventory;
    }
    inventory_sample_sum_ += static_cast<long double>(absolute_inventory);
}

const BacktestMetrics& BacktestMetricsCollector::current() const noexcept {
    return current_;
}

std::uint64_t BacktestMetricsCollector::observedFills() const noexcept {
    return observed_fills_;
}

std::uint64_t BacktestMetricsCollector::observedMarkets() const noexcept {
    return observed_markets_;
}

std::uint64_t BacktestMetricsCollector::lastTimestamp() const noexcept {
    return last_timestamp_;
}

std::int64_t BacktestMetricsCollector::maxInventory() const noexcept {
    return max_inventory_;
}

long double BacktestMetricsCollector::averageInventory() const noexcept {
    if (observed_markets_ == 0) {
        return 0.0L;
    }
    return inventory_sample_sum_ / static_cast<long double>(observed_markets_);
}

} // namespace md
