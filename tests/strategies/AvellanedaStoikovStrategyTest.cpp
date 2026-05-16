#include "TestSupport.hpp"

#include "strategies/AvellanedaStoikovStrategy.hpp"

#include <variant>
#include <vector>

namespace md::test {
namespace {

AvellanedaStoikovConfig config(
    long double gamma = 0.1L,
    long double sigma = 1.0L,
    std::int64_t tick_size = 1,
    std::int64_t max_inventory = 100,
    std::uint64_t order_size = 10
) {
    return AvellanedaStoikovConfig{
        .instrument_id = 42,
        .order_size = order_size,
        .gamma = gamma,
        .sigma = sigma,
        .k = 1.0L,
        .horizon_seconds = 1.0L,
        .tick_size = tick_size,
        .max_inventory = max_inventory,
        .quote_interval_events = 1
    };
}

MarketDataEvent marketEvent(std::uint64_t instrument_id = 42) {
    MarketDataEvent event;
    event.instrument_id = instrument_id;
    event.timestamp = 100;
    return event;
}

MarketView validMarket(std::int64_t mid_price = 100, std::uint64_t instrument_id = 42) {
    MarketView market;
    market.instrument_id = instrument_id;
    market.best_bid = mid_price - 1;
    market.best_ask = mid_price + 1;
    market.best_bid_size = 10;
    market.best_ask_size = 10;
    market.mid_price = mid_price;
    market.spread = 2;
    market.microprice = mid_price;
    return market;
}

MarketView emptyMarket(std::uint64_t instrument_id = 42) {
    MarketView market;
    market.instrument_id = instrument_id;
    return market;
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
    AvellanedaStoikovStrategy& strategy,
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

} // namespace

void testAsStrategyQuotesBidAndAskOnValidMarket() {
    AvellanedaStoikovStrategy strategy{config()};
    Portfolio portfolio;
    OrderManager orders;

    const auto actions = runStrategy(strategy, validMarket(100), portfolio, orders);

    require(actions.size() == 2, "as_strategy_quotes_bid_and_ask_on_valid_market: action count");
    const auto* bid = requirePlaceAction(actions[0], "as_strategy_quotes_bid_and_ask_on_valid_market bid");
    const auto* ask = requirePlaceAction(actions[1], "as_strategy_quotes_bid_and_ask_on_valid_market ask");
    require(bid->side == SimOrderSide::Buy, "as_strategy_quotes_bid_and_ask_on_valid_market: bid side");
    require(ask->side == SimOrderSide::Sell, "as_strategy_quotes_bid_and_ask_on_valid_market: ask side");
    require(bid->price < 100, "as_strategy_quotes_bid_and_ask_on_valid_market: bid below mid");
    require(ask->price > 100, "as_strategy_quotes_bid_and_ask_on_valid_market: ask above mid");
    require(ask->price > bid->price, "as_strategy_quotes_bid_and_ask_on_valid_market: positive spread");
}

void testAsStrategyDoesNotQuoteEmptyBook() {
    AvellanedaStoikovStrategy strategy{config()};
    Portfolio portfolio;
    OrderManager orders;

    const auto actions = runStrategy(strategy, emptyMarket(), portfolio, orders);

    require(actions.empty(), "as_strategy_does_not_quote_empty_book: no actions");
}

void testAsReservationPriceDecreasesWhenInventoryLong() {
    AvellanedaStoikovStrategy strategy{config()};

    const auto reservation_price = strategy.reservationPrice(100, 10);

    require(reservation_price < 100.0L, "as_reservation_price_decreases_when_inventory_long: below mid");
}

void testAsReservationPriceIncreasesWhenInventoryShort() {
    AvellanedaStoikovStrategy strategy{config()};

    const auto reservation_price = strategy.reservationPrice(100, -10);

    require(reservation_price > 100.0L, "as_reservation_price_increases_when_inventory_short: above mid");
}

void testAsSpreadIncreasesWhenGammaIncreases() {
    AvellanedaStoikovStrategy low_gamma{config(0.1L, 1.0L)};
    AvellanedaStoikovStrategy high_gamma{config(0.2L, 1.0L)};

    require(
        high_gamma.optimalSpread() > low_gamma.optimalSpread(),
        "as_spread_increases_when_gamma_increases"
    );
}

void testAsSpreadIncreasesWhenSigmaIncreases() {
    AvellanedaStoikovStrategy low_sigma{config(0.1L, 1.0L)};
    AvellanedaStoikovStrategy high_sigma{config(0.1L, 2.0L)};

    require(
        high_sigma.optimalSpread() > low_sigma.optimalSpread(),
        "as_spread_increases_when_sigma_increases"
    );
}

void testAsStrategyRespectsMaxInventory() {
    AvellanedaStoikovStrategy strategy{config(0.1L, 1.0L, 1, 10, 1)};
    Portfolio portfolio;
    portfolio.applyFill(fill(SimOrderSide::Buy, 10));
    OrderManager orders;

    const auto actions = runStrategy(strategy, validMarket(100), portfolio, orders);

    require(actions.size() == 1, "as_strategy_respects_max_inventory: one action");
    const auto* ask = requirePlaceAction(actions.front(), "as_strategy_respects_max_inventory");
    require(ask->side == SimOrderSide::Sell, "as_strategy_respects_max_inventory: only sell");
}

void testAsStrategyRoundsQuotesToTick() {
    AvellanedaStoikovStrategy strategy{config(0.1L, 1.0L, 5)};
    Portfolio portfolio;
    OrderManager orders;

    const auto actions = runStrategy(strategy, validMarket(100), portfolio, orders);

    require(actions.size() == 2, "as_strategy_rounds_quotes_to_tick: action count");
    const auto* bid = requirePlaceAction(actions[0], "as_strategy_rounds_quotes_to_tick bid");
    const auto* ask = requirePlaceAction(actions[1], "as_strategy_rounds_quotes_to_tick ask");
    require(bid->price % 5 == 0, "as_strategy_rounds_quotes_to_tick: bid tick");
    require(ask->price % 5 == 0, "as_strategy_rounds_quotes_to_tick: ask tick");
    require(bid->price == 95, "as_strategy_rounds_quotes_to_tick: rounded bid");
    require(ask->price == 105, "as_strategy_rounds_quotes_to_tick: rounded ask");
}

} // namespace md::test
