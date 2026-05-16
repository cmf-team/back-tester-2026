#include "TestSupport.hpp"

#include "book/LimitOrderBook.hpp"
#include "strategies/AvellanedaStoikovStrategy.hpp"
#include "strategies/MicropriceAvellanedaStoikovStrategy.hpp"

#include <variant>
#include <vector>

namespace md::test {
namespace {

std::int64_t P(std::int64_t integer_price) {
    return integer_price * 1'000LL;
}

MicropriceAvellanedaStoikovConfig microConfig(std::int64_t tick_size = 100) {
    return MicropriceAvellanedaStoikovConfig{
        .instrument_id = 42,
        .order_size = 10,
        .gamma = 0.1L,
        .sigma = 1.0L,
        .k = 1.0L,
        .horizon_seconds = 1.0L,
        .tick_size = tick_size,
        .max_inventory = 100,
        .quote_interval_events = 1,
        .use_imbalance_skew = false,
        .imbalance_alpha_ticks = 0.0L
    };
}

AvellanedaStoikovConfig baseConfig(std::int64_t tick_size = 100) {
    return AvellanedaStoikovConfig{
        .instrument_id = 42,
        .order_size = 10,
        .gamma = 0.1L,
        .sigma = 1.0L,
        .k = 1.0L,
        .horizon_seconds = 1.0L,
        .tick_size = tick_size,
        .max_inventory = 100,
        .quote_interval_events = 1
    };
}

MarketDataEvent add(
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    MarketDataEvent event;
    event.order_id = order_id;
    event.side = side;
    event.price = price;
    event.size = size;
    event.action = Action::Add;
    event.instrument_id = 42;
    event.timestamp = 100;
    return event;
}

MarketDataEvent marketEvent() {
    MarketDataEvent event;
    event.instrument_id = 42;
    event.timestamp = 100;
    return event;
}

MarketView imbalancedMarket(std::uint64_t bid_size, std::uint64_t ask_size) {
    LimitOrderBook book{42};
    book.apply(add(1, Side::Bid, P(100), bid_size));
    book.apply(add(2, Side::Ask, P(102), ask_size));
    return makeMarketView(book, 100);
}

std::vector<StrategyAction> runMicroStrategy(
    MicropriceAvellanedaStoikovStrategy& strategy,
    const MarketView& market
) {
    Portfolio portfolio;
    OrderManager orders;
    return strategy.onMarketData(marketEvent(), market, portfolio, orders);
}

std::vector<StrategyAction> runBaseStrategy(
    AvellanedaStoikovStrategy& strategy,
    const MarketView& market
) {
    Portfolio portfolio;
    OrderManager orders;
    return strategy.onMarketData(marketEvent(), market, portfolio, orders);
}

const PlaceOrderAction* requirePlaceAction(const StrategyAction& action, const std::string& case_name) {
    const auto* place = std::get_if<PlaceOrderAction>(&action);
    require(place != nullptr, case_name + ": expected place action");
    return place;
}

} // namespace

void testMicropriceAsUsesMicropriceNotMid() {
    const auto market = imbalancedMarket(90, 10);
    MicropriceAvellanedaStoikovStrategy strategy{microConfig()};

    const auto fair_price = strategy.fairPrice(market);
    const auto reservation_price = strategy.reservationPrice(fair_price, 0);

    require(market.mid_price == P(101), "microprice_as_uses_microprice_not_mid: mid");
    require(market.microprice == 101'800, "microprice_as_uses_microprice_not_mid: microprice");
    require(fair_price == 101'800.0L, "microprice_as_uses_microprice_not_mid: fair price");
    require(reservation_price == 101'800.0L, "microprice_as_uses_microprice_not_mid: reservation");
    require(reservation_price > static_cast<long double>(*market.mid_price), "microprice_as_uses_microprice_not_mid: above mid");
}

void testMicropriceMovesTowardAskWhenBidSizeDominates() {
    const auto market = imbalancedMarket(90, 10);

    require(market.mid_price == P(101), "microprice_moves_toward_ask_when_bid_size_dominates: mid");
    require(market.microprice == 101'800, "microprice_moves_toward_ask_when_bid_size_dominates: microprice");
    require(*market.microprice > *market.mid_price, "microprice_moves_toward_ask_when_bid_size_dominates: above mid");
    require(*market.microprice < *market.best_ask, "microprice_moves_toward_ask_when_bid_size_dominates: below ask");
}

void testMicropriceMovesTowardBidWhenAskSizeDominates() {
    const auto market = imbalancedMarket(10, 90);

    require(market.mid_price == P(101), "microprice_moves_toward_bid_when_ask_size_dominates: mid");
    require(market.microprice == 100'200, "microprice_moves_toward_bid_when_ask_size_dominates: microprice");
    require(*market.microprice < *market.mid_price, "microprice_moves_toward_bid_when_ask_size_dominates: below mid");
    require(*market.microprice > *market.best_bid, "microprice_moves_toward_bid_when_ask_size_dominates: above bid");
}

void testMicropriceAsQuotesDifferFromBaseAsOnImbalancedBook() {
    const auto market = imbalancedMarket(90, 10);
    AvellanedaStoikovStrategy base_strategy{baseConfig()};
    MicropriceAvellanedaStoikovStrategy micro_strategy{microConfig()};

    const auto base_actions = runBaseStrategy(base_strategy, market);
    const auto micro_actions = runMicroStrategy(micro_strategy, market);

    require(base_actions.size() == 2, "microprice_as_quotes_differ_from_base_as_on_imbalanced_book: base count");
    require(micro_actions.size() == 2, "microprice_as_quotes_differ_from_base_as_on_imbalanced_book: micro count");
    const auto* base_bid = requirePlaceAction(base_actions[0], "microprice_as_quotes_differ_from_base_as_on_imbalanced_book base bid");
    const auto* micro_bid = requirePlaceAction(micro_actions[0], "microprice_as_quotes_differ_from_base_as_on_imbalanced_book micro bid");
    const auto* base_ask = requirePlaceAction(base_actions[1], "microprice_as_quotes_differ_from_base_as_on_imbalanced_book base ask");
    const auto* micro_ask = requirePlaceAction(micro_actions[1], "microprice_as_quotes_differ_from_base_as_on_imbalanced_book micro ask");

    require(micro_bid->price > base_bid->price, "microprice_as_quotes_differ_from_base_as_on_imbalanced_book: bid higher");
    require(micro_ask->price > base_ask->price, "microprice_as_quotes_differ_from_base_as_on_imbalanced_book: ask higher");
}

void testMicropriceAsRespectsInventorySkew() {
    const auto market = imbalancedMarket(90, 10);
    MicropriceAvellanedaStoikovStrategy strategy{microConfig()};
    const auto fair_price = strategy.fairPrice(market);

    const auto long_reservation = strategy.reservationPrice(fair_price, 10);
    const auto short_reservation = strategy.reservationPrice(fair_price, -10);

    require(long_reservation < fair_price, "microprice_as_respects_inventory_skew: long lowers reservation");
    require(short_reservation > fair_price, "microprice_as_respects_inventory_skew: short raises reservation");
}

void testMicropriceAsOptionalImbalanceSkewMovesFairPrice() {
    const auto market = imbalancedMarket(90, 10);
    auto config = microConfig();
    config.use_imbalance_skew = true;
    config.imbalance_alpha_ticks = 2.0L;
    MicropriceAvellanedaStoikovStrategy strategy{config};

    const auto skewed_fair_price = strategy.fairPrice(market);

    require(strategy.imbalance(market) > 0.79L, "microprice_as_optional_imbalance_skew_moves_fair_price: imbalance lower");
    require(strategy.imbalance(market) < 0.81L, "microprice_as_optional_imbalance_skew_moves_fair_price: imbalance upper");
    require(
        skewed_fair_price == 101'960.0L,
        "microprice_as_optional_imbalance_skew_moves_fair_price: skewed fair price"
    );
}

} // namespace md::test
