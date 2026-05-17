#include "catch2/catch_all.hpp"
#include "common/MarketDataEvent.hpp"
#include "order_book/AbseilOrderBook.hpp"
#include "order_book/MapOrderBook.hpp"

#include <sstream>
#include <string>

using namespace cmf;

static MarketDataEvent make_add(uint64_t order_id, Side side, int64_t price,
                                uint32_t size)
{
    MarketDataEvent e{};
    e.order_id = order_id;
    e.action = Action::Add;
    e.side = side;
    e.price = price;
    e.size = size;
    return e;
}

static MarketDataEvent make_cancel(uint64_t order_id, uint32_t remaining_size)
{
    MarketDataEvent e{};
    e.order_id = order_id;
    e.action = Action::Cancel;
    e.size = remaining_size;
    return e;
}

static MarketDataEvent make_modify(uint64_t order_id, Side side, int64_t price,
                                   uint32_t size)
{
    MarketDataEvent e{};
    e.order_id = order_id;
    e.action = Action::Modify;
    e.side = side;
    e.price = price;
    e.size = size;
    return e;
}

static MarketDataEvent make_fill(uint64_t order_id, uint32_t remaining_size)
{
    MarketDataEvent e{};
    e.order_id = order_id;
    e.action = Action::Fill;
    e.size = remaining_size;
    return e;
}

static MarketDataEvent make_trade()
{
    MarketDataEvent e{};
    e.action = Action::Trade;
    return e;
}

static MarketDataEvent make_clear()
{
    MarketDataEvent e{};
    e.action = Action::Clear;
    return e;
}

// ---------------------------------------------------------------------------

TEMPLATE_TEST_CASE("OrderBook - empty book", "[OrderBook]", MapOrderBook,
                   AbseilOrderBook)
{
    TestType book;
    REQUIRE_FALSE(book.best_price(Side::Buy).has_value());
    REQUIRE_FALSE(book.best_price(Side::Sell).has_value());
    REQUIRE(book.volume_at(Side::Buy, 100'000'000'000LL) == 0);
    REQUIRE(book.volume_at(Side::Sell, 100'000'000'000LL) == 0);
    REQUIRE(book.empty(Side::Buy));
    REQUIRE(book.empty(Side::Sell));
}

TEMPLATE_TEST_CASE("OrderBook - add single bid", "[OrderBook]", MapOrderBook,
                   AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    REQUIRE(book.best_price(Side::Buy) == std::optional{5000'000'000'000LL});
    REQUIRE(book.volume_at(Side::Buy, 5000'000'000'000LL) == 10);
    REQUIRE_FALSE(book.empty(Side::Buy));
    REQUIRE(book.empty(Side::Sell));
}

TEMPLATE_TEST_CASE("OrderBook - add single ask", "[OrderBook]", MapOrderBook,
                   AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Sell, 5010'000'000'000LL, 5));
    REQUIRE(book.best_price(Side::Sell) == std::optional{5010'000'000'000LL});
    REQUIRE(book.volume_at(Side::Sell, 5010'000'000'000LL) == 5);
    REQUIRE(book.empty(Side::Buy));
    REQUIRE_FALSE(book.empty(Side::Sell));
}

TEMPLATE_TEST_CASE("OrderBook - multiple bid levels, best bid is highest",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 4990'000'000'000LL, 3));
    book.apply(make_add(2, Side::Buy, 5000'000'000'000LL, 7));
    book.apply(make_add(3, Side::Buy, 4980'000'000'000LL, 2));
    REQUIRE(book.best_price(Side::Buy) == std::optional{5000'000'000'000LL});

    auto levels = book.side_levels(Side::Buy);
    auto it = levels.begin();
    REQUIRE(it->first == 5000'000'000'000LL);
    REQUIRE(it->second == 7);
    ++it;
    REQUIRE(it->first == 4990'000'000'000LL);
    ++it;
    REQUIRE(it->first == 4980'000'000'000LL);
}

TEMPLATE_TEST_CASE("OrderBook - multiple ask levels, best ask is lowest",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Sell, 5020'000'000'000LL, 1));
    book.apply(make_add(2, Side::Sell, 5010'000'000'000LL, 4));
    book.apply(make_add(3, Side::Sell, 5030'000'000'000LL, 2));
    REQUIRE(book.best_price(Side::Sell) == std::optional{5010'000'000'000LL});

    auto levels = book.side_levels(Side::Sell);
    auto it = levels.begin();
    REQUIRE(it->first == 5010'000'000'000LL);
    REQUIRE(it->second == 4);
}

TEMPLATE_TEST_CASE("OrderBook - aggregate qty at same price level",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    book.apply(make_add(2, Side::Buy, 5000'000'000'000LL, 5));
    REQUIRE(book.volume_at(Side::Buy, 5000'000'000'000LL) == 15);
}

TEMPLATE_TEST_CASE("OrderBook - full cancel removes order and level",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    book.apply(make_cancel(1, 0)); // remaining = 0 → fully cancel
    REQUIRE(book.empty(Side::Buy));
    REQUIRE_FALSE(book.best_price(Side::Buy).has_value());
}

TEMPLATE_TEST_CASE("OrderBook - partial cancel reduces level qty",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    book.apply(make_cancel(1, 4)); // remaining = 4 → reduce by 6
    REQUIRE(book.volume_at(Side::Buy, 5000'000'000'000LL) == 4);
    REQUIRE(book.best_price(Side::Buy).has_value());
}

TEMPLATE_TEST_CASE("OrderBook - modify changes price level", "[OrderBook]",
                   MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    book.apply(make_modify(1, Side::Buy, 4990'000'000'000LL, 10));
    REQUIRE(book.volume_at(Side::Buy, 5000'000'000'000LL) == 0);
    REQUIRE(book.volume_at(Side::Buy, 4990'000'000'000LL) == 10);
    REQUIRE(book.best_price(Side::Buy) == std::optional{4990'000'000'000LL});
}

TEMPLATE_TEST_CASE("OrderBook - modify changes size at same price",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    book.apply(make_modify(1, Side::Buy, 5000'000'000'000LL, 3));
    REQUIRE(book.volume_at(Side::Buy, 5000'000'000'000LL) == 3);
}

TEMPLATE_TEST_CASE("OrderBook - trade event leaves book unchanged",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Sell, 5010'000'000'000LL, 5));
    book.apply(make_trade());
    REQUIRE(book.volume_at(Side::Sell, 5010'000'000'000LL) == 5);
}

TEMPLATE_TEST_CASE("OrderBook - fill does not affect the book", "[OrderBook]",
                   MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Sell, 5010'000'000'000LL, 5));
    book.apply(make_fill(1, 0)); // fill is a no-op
    REQUIRE(book.volume_at(Side::Sell, 5010'000'000'000LL) == 5);
    REQUIRE_FALSE(book.empty(Side::Sell));
}

TEMPLATE_TEST_CASE("OrderBook - fill does not affect orders at shared level",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    constexpr int64_t P = 5000'000'000'000LL;
    book.apply(make_add(1, Side::Buy, P, 10));
    book.apply(make_add(2, Side::Buy, P, 5)); // second order at same level
    REQUIRE(book.volume_at(Side::Buy, P) == 15);

    book.apply(make_fill(1, 0)); // fill is a no-op
    // both orders remain unchanged
    REQUIRE(book.volume_at(Side::Buy, P) == 15);
    REQUIRE(book.best_price(Side::Buy) == std::optional{P});
}

TEMPLATE_TEST_CASE("OrderBook - fill does not modify any price levels",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    constexpr int64_t P1 = 5000'000'000'000LL;
    constexpr int64_t P2 = 4990'000'000'000LL;
    book.apply(make_add(1, Side::Buy, P1, 10));
    book.apply(make_add(2, Side::Buy, P2, 5));

    book.apply(make_fill(1, 0));                  // fill is a no-op
    REQUIRE(book.volume_at(Side::Buy, P1) == 10); // P1 unchanged
    REQUIRE(book.volume_at(Side::Buy, P2) == 5);  // P2 unchanged
    REQUIRE(book.best_price(Side::Buy) == std::optional{P1});
}

TEMPLATE_TEST_CASE("OrderBook - consecutive fills leave book unchanged",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    constexpr int64_t P = 5000'000'000'000LL;
    book.apply(make_add(1, Side::Buy, P, 10));

    book.apply(make_fill(1, 7)); // fill is a no-op
    REQUIRE(book.volume_at(Side::Buy, P) == 10);

    book.apply(make_fill(1, 0)); // fill is a no-op
    REQUIRE(book.volume_at(Side::Buy, P) == 10);
}

TEMPLATE_TEST_CASE("OrderBook - clear wipes both sides", "[OrderBook]",
                   MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    book.apply(make_add(2, Side::Sell, 5010'000'000'000LL, 5));
    book.apply(make_clear());
    REQUIRE(book.empty(Side::Buy));
    REQUIRE(book.empty(Side::Sell));
}

TEMPLATE_TEST_CASE("OrderBook - print_snapshot contains expected lines",
                   "[OrderBook]", MapOrderBook, AbseilOrderBook)
{
    TestType book;
    book.apply(make_add(1, Side::Buy, 5000'000'000'000LL, 10));
    book.apply(make_add(2, Side::Sell, 5010'000'000'000LL, 5));

    std::ostringstream oss;
    book.print_snapshot(oss);
    const std::string out = oss.str();

    REQUIRE(out.find("BUY") != std::string::npos);
    REQUIRE(out.find("SELL") != std::string::npos);
    REQUIRE(out.find("5000000000000") != std::string::npos);
    REQUIRE(out.find("5010000000000") != std::string::npos);
}
