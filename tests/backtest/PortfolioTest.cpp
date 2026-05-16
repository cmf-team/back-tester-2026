#include "TestSupport.hpp"

#include "backtest/BacktestMetrics.hpp"
#include "backtest/Portfolio.hpp"

#include <vector>

namespace md::test {
namespace {

Fill fill(
    SimOrderSide side,
    std::uint64_t instrument_id,
    std::int64_t price,
    std::uint64_t quantity
) {
    return Fill{
        .client_order_id = 1,
        .instrument_id = instrument_id,
        .side = side,
        .price = price,
        .quantity = quantity,
        .timestamp = 12345
    };
}

MarketView marketWithMid(std::uint64_t instrument_id, std::int64_t mid_price) {
    MarketView market;
    market.instrument_id = instrument_id;
    market.mid_price = mid_price;
    return market;
}

const InstrumentPosition& requirePosition(const Portfolio& portfolio, std::uint64_t instrument_id) {
    const auto* position = portfolio.findPosition(instrument_id);
    require(position != nullptr, "portfolio position exists");
    return *position;
}

} // namespace

void testPortfolioBuyFillUpdatesInventoryAndCash() {
    Portfolio portfolio;

    portfolio.applyFill(fill(SimOrderSide::Buy, 1, 100, 10));

    const auto& position = requirePosition(portfolio, 1);
    require(position.inventory == 10, "portfolio_buy_fill_updates_inventory_and_cash: inventory");
    require(position.cash == -1000.0L, "portfolio_buy_fill_updates_inventory_and_cash: cash");
    require(position.turnover == 1000.0L, "portfolio_buy_fill_updates_inventory_and_cash: turnover");
    require(position.fills == 1, "portfolio_buy_fill_updates_inventory_and_cash: fills");
}

void testPortfolioSellFillUpdatesInventoryAndCash() {
    Portfolio portfolio;

    portfolio.applyFill(fill(SimOrderSide::Sell, 1, 105, 7));

    const auto& position = requirePosition(portfolio, 1);
    require(position.inventory == -7, "portfolio_sell_fill_updates_inventory_and_cash: inventory");
    require(position.cash == 735.0L, "portfolio_sell_fill_updates_inventory_and_cash: cash");
    require(position.turnover == 735.0L, "portfolio_sell_fill_updates_inventory_and_cash: turnover");
    require(position.fills == 1, "portfolio_sell_fill_updates_inventory_and_cash: fills");
}

void testPortfolioRoundTripRealizesPositivePnl() {
    Portfolio portfolio;

    portfolio.applyFill(fill(SimOrderSide::Buy, 1, 100, 10));
    portfolio.applyFill(fill(SimOrderSide::Sell, 1, 101, 10));

    const auto& position = requirePosition(portfolio, 1);
    const auto metrics = makeBacktestMetrics(portfolio, std::vector<MarketView>{marketWithMid(1, 101)});

    require(position.inventory == 0, "portfolio_round_trip_realizes_positive_pnl: flat inventory");
    require(position.cash == 10.0L, "portfolio_round_trip_realizes_positive_pnl: realized cash");
    require(metrics.mtm_pnl == 10.0L, "portfolio_round_trip_realizes_positive_pnl: mtm pnl");
}

void testPortfolioTurnoverAccumulatesAbsoluteNotional() {
    Portfolio portfolio;

    portfolio.applyFill(fill(SimOrderSide::Buy, 1, 100, 10));
    portfolio.applyFill(fill(SimOrderSide::Sell, 1, 101, 10));

    const auto& position = requirePosition(portfolio, 1);
    const auto metrics = makeBacktestMetrics(portfolio, std::vector<MarketView>{marketWithMid(1, 101)});

    require(position.turnover == 2010.0L, "portfolio_turnover_accumulates_absolute_notional: position turnover");
    require(metrics.turnover == 2010.0L, "portfolio_turnover_accumulates_absolute_notional: metrics turnover");
}

void testPortfolioMtmUsesMidPrice() {
    Portfolio portfolio;

    portfolio.applyFill(fill(SimOrderSide::Buy, 1, 100, 10));

    const auto& position = requirePosition(portfolio, 1);
    const auto mtm_pnl = markToMarketPnl(position, 99, portfolio.multiplier());
    const auto metrics = makeBacktestMetrics(portfolio, std::vector<MarketView>{marketWithMid(1, 99)});

    require(position.inventory == 10, "portfolio_mtm_uses_mid_price: inventory");
    require(position.cash == -1000.0L, "portfolio_mtm_uses_mid_price: cash");
    require(mtm_pnl == -10.0L, "portfolio_mtm_uses_mid_price: free function mtm pnl");
    require(metrics.mtm_pnl == -10.0L, "portfolio_mtm_uses_mid_price: metrics mtm pnl");
}

} // namespace md::test
