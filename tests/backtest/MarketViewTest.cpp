#include "TestSupport.hpp"

#include "backtest/MarketView.hpp"

namespace md::test {
namespace {

std::int64_t P(std::int64_t integer_price) {
    return integer_price * 1'000'000'000LL;
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
    return event;
}

LimitOrderBook bookWithBestLevels(
    std::int64_t bid_price,
    std::uint64_t bid_size,
    std::int64_t ask_price,
    std::uint64_t ask_size
) {
    LimitOrderBook book{42};
    book.apply(add(1, Side::Bid, bid_price, bid_size));
    book.apply(add(2, Side::Ask, ask_price, ask_size));
    return book;
}

} // namespace

void testMarketViewEmptyBookHasNoPrices() {
    LimitOrderBook book{42};

    const auto view = makeMarketView(book, 12345);

    require(view.instrument_id == 42, "market_view_empty_book_has_no_prices: instrument id");
    require(view.timestamp == 12345, "market_view_empty_book_has_no_prices: timestamp");
    require(!view.best_bid.has_value(), "market_view_empty_book_has_no_prices: no best bid");
    require(!view.best_ask.has_value(), "market_view_empty_book_has_no_prices: no best ask");
    require(view.best_bid_size == 0, "market_view_empty_book_has_no_prices: no best bid size");
    require(view.best_ask_size == 0, "market_view_empty_book_has_no_prices: no best ask size");
    require(!view.mid_price.has_value(), "market_view_empty_book_has_no_prices: no mid");
    require(!view.spread.has_value(), "market_view_empty_book_has_no_prices: no spread");
    require(!view.microprice.has_value(), "market_view_empty_book_has_no_prices: no microprice");
}

void testMarketViewComputesMidPrice() {
    const auto book = bookWithBestLevels(P(100), 10, P(102), 10);

    const auto view = makeMarketView(book, 12345);

    require(view.best_bid == P(100), "market_view_computes_mid_price: best bid");
    require(view.best_ask == P(102), "market_view_computes_mid_price: best ask");
    require(view.best_bid_size == 10, "market_view_computes_mid_price: best bid size");
    require(view.best_ask_size == 10, "market_view_computes_mid_price: best ask size");
    require(view.mid_price == P(101), "market_view_computes_mid_price: mid price");
}

void testMarketViewComputesSpread() {
    const auto book = bookWithBestLevels(P(100), 10, P(102), 10);

    const auto view = makeMarketView(book, 12345);

    require(view.spread == P(2), "market_view_computes_spread: spread");
}

void testMarketViewComputesMicropriceBalancedBook() {
    const auto book = bookWithBestLevels(P(100), 10, P(102), 10);

    const auto view = makeMarketView(book, 12345);

    require(view.microprice == P(101), "market_view_computes_microprice_balanced_book: microprice");
}

void testMarketViewComputesMicropriceImbalancedBook() {
    const auto book = bookWithBestLevels(P(100), 90, P(102), 10);

    const auto view = makeMarketView(book, 12345);

    require(
        view.microprice == 101'800'000'000LL,
        "market_view_computes_microprice_imbalanced_book: microprice"
    );
}

} // namespace md::test
