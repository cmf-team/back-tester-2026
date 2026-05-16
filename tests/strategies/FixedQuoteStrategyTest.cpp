#include "TestSupport.hpp"

#include "strategies/FixedQuoteStrategy.hpp"

#include <variant>
#include <vector>

namespace md::test {
namespace {

MarketDataEvent marketEvent(std::uint64_t instrument_id = 42) {
    MarketDataEvent event;
    event.instrument_id = instrument_id;
    event.timestamp = 100;
    return event;
}

MarketView validMarket(std::uint64_t instrument_id = 42) {
    MarketView market;
    market.instrument_id = instrument_id;
    market.best_bid = 100;
    market.best_ask = 102;
    market.best_bid_size = 10;
    market.best_ask_size = 10;
    market.mid_price = 101;
    market.spread = 2;
    market.microprice = 101;
    return market;
}

MarketView emptyMarket(std::uint64_t instrument_id = 42) {
    MarketView market;
    market.instrument_id = instrument_id;
    return market;
}

FixedQuoteStrategyConfig config(
    std::uint64_t order_size = 10,
    std::int64_t max_inventory = 100
) {
    return FixedQuoteStrategyConfig{
        .instrument_id = 42,
        .order_size = order_size,
        .quote_offset_ticks = 1,
        .quote_interval_events = 1,
        .max_inventory = max_inventory
    };
}

Fill fill(SimOrderSide side, std::uint64_t quantity) {
    return Fill{
        .client_order_id = 1,
        .instrument_id = 42,
        .side = side,
        .price = 100,
        .quantity = quantity,
        .timestamp = 1
    };
}

std::vector<StrategyAction> runStrategy(
    FixedQuoteStrategy& strategy,
    const MarketView& market,
    const Portfolio& portfolio,
    const OrderManager& orders
) {
    return strategy.onMarketData(marketEvent(market.instrument_id), market, portfolio, orders);
}

const PlaceOrderAction* requirePlaceAction(const StrategyAction& action, const std::string& case_name) {
    const auto* place = std::get_if<PlaceOrderAction>(&action);
    require(place != nullptr, case_name + ": expected place action");
    return place;
}

const CancelOrderAction* requireCancelAction(const StrategyAction& action, const std::string& case_name) {
    const auto* cancel = std::get_if<CancelOrderAction>(&action);
    require(cancel != nullptr, case_name + ": expected cancel action");
    return cancel;
}

} // namespace

void testFixedQuoteStrategyPlacesTwoOrdersOnValidMarket() {
    FixedQuoteStrategy strategy{config()};
    Portfolio portfolio;
    OrderManager orders;

    const auto actions = runStrategy(strategy, validMarket(), portfolio, orders);

    require(actions.size() == 2, "fixed_quote_strategy_places_two_orders_on_valid_market: action count");
    const auto* buy = requirePlaceAction(actions[0], "fixed_quote_strategy_places_two_orders_on_valid_market buy");
    const auto* sell = requirePlaceAction(actions[1], "fixed_quote_strategy_places_two_orders_on_valid_market sell");

    require(buy->instrument_id == 42, "fixed_quote_strategy_places_two_orders_on_valid_market: buy instrument");
    require(buy->side == SimOrderSide::Buy, "fixed_quote_strategy_places_two_orders_on_valid_market: buy side");
    require(buy->price == 100, "fixed_quote_strategy_places_two_orders_on_valid_market: buy price");
    require(buy->quantity == 10, "fixed_quote_strategy_places_two_orders_on_valid_market: buy quantity");
    require(sell->instrument_id == 42, "fixed_quote_strategy_places_two_orders_on_valid_market: sell instrument");
    require(sell->side == SimOrderSide::Sell, "fixed_quote_strategy_places_two_orders_on_valid_market: sell side");
    require(sell->price == 102, "fixed_quote_strategy_places_two_orders_on_valid_market: sell price");
    require(sell->quantity == 10, "fixed_quote_strategy_places_two_orders_on_valid_market: sell quantity");
}

void testFixedQuoteStrategyDoesNotQuoteEmptyBook() {
    FixedQuoteStrategy strategy{config()};
    Portfolio portfolio;
    OrderManager orders;

    const auto actions = runStrategy(strategy, emptyMarket(), portfolio, orders);

    require(actions.empty(), "fixed_quote_strategy_does_not_quote_empty_book: no actions");
}

void testFixedQuoteStrategyRespectsMaxInventoryLong() {
    FixedQuoteStrategy strategy{config(1, 10)};
    Portfolio portfolio;
    portfolio.applyFill(fill(SimOrderSide::Buy, 10));
    OrderManager orders;

    const auto actions = runStrategy(strategy, validMarket(), portfolio, orders);

    require(actions.size() == 1, "fixed_quote_strategy_respects_max_inventory_long: one action");
    const auto* sell = requirePlaceAction(actions.front(), "fixed_quote_strategy_respects_max_inventory_long");
    require(sell->side == SimOrderSide::Sell, "fixed_quote_strategy_respects_max_inventory_long: only sell");
    require(sell->price == 102, "fixed_quote_strategy_respects_max_inventory_long: sell price");
}

void testFixedQuoteStrategyRespectsMaxInventoryShort() {
    FixedQuoteStrategy strategy{config(1, 10)};
    Portfolio portfolio;
    portfolio.applyFill(fill(SimOrderSide::Sell, 10));
    OrderManager orders;

    const auto actions = runStrategy(strategy, validMarket(), portfolio, orders);

    require(actions.size() == 1, "fixed_quote_strategy_respects_max_inventory_short: one action");
    const auto* buy = requirePlaceAction(actions.front(), "fixed_quote_strategy_respects_max_inventory_short");
    require(buy->side == SimOrderSide::Buy, "fixed_quote_strategy_respects_max_inventory_short: only buy");
    require(buy->price == 100, "fixed_quote_strategy_respects_max_inventory_short: buy price");
}

void testFixedQuoteStrategyCancelsOldQuotesBeforeRequoting() {
    FixedQuoteStrategy strategy{config()};
    Portfolio portfolio;
    OrderManager orders;
    const auto old_buy = orders.placeLimitOrder(42, SimOrderSide::Buy, 99, 10, 1);
    const auto old_sell = orders.placeLimitOrder(42, SimOrderSide::Sell, 103, 10, 1);

    const auto actions = runStrategy(strategy, validMarket(), portfolio, orders);

    require(actions.size() == 4, "fixed_quote_strategy_cancels_old_quotes_before_requoting: action count");
    const auto* first_cancel = requireCancelAction(actions[0], "fixed_quote_strategy_cancels_old_quotes_before_requoting first");
    const auto* second_cancel = requireCancelAction(actions[1], "fixed_quote_strategy_cancels_old_quotes_before_requoting second");
    const auto* buy = requirePlaceAction(actions[2], "fixed_quote_strategy_cancels_old_quotes_before_requoting buy");
    const auto* sell = requirePlaceAction(actions[3], "fixed_quote_strategy_cancels_old_quotes_before_requoting sell");

    require(
        first_cancel->client_order_id == old_buy || first_cancel->client_order_id == old_sell,
        "fixed_quote_strategy_cancels_old_quotes_before_requoting: first cancel id"
    );
    require(
        second_cancel->client_order_id == old_buy || second_cancel->client_order_id == old_sell,
        "fixed_quote_strategy_cancels_old_quotes_before_requoting: second cancel id"
    );
    require(
        first_cancel->client_order_id != second_cancel->client_order_id,
        "fixed_quote_strategy_cancels_old_quotes_before_requoting: distinct cancel ids"
    );
    require(buy->side == SimOrderSide::Buy, "fixed_quote_strategy_cancels_old_quotes_before_requoting: buy side");
    require(buy->price == 100, "fixed_quote_strategy_cancels_old_quotes_before_requoting: buy price");
    require(sell->side == SimOrderSide::Sell, "fixed_quote_strategy_cancels_old_quotes_before_requoting: sell side");
    require(sell->price == 102, "fixed_quote_strategy_cancels_old_quotes_before_requoting: sell price");
}

} // namespace md::test
