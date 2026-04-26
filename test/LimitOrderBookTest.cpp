#include "lob/LimitOrderBook.hpp"

#include "catch2/catch_all.hpp"

using namespace cmf;

namespace {
LimitOrderBook::ScaledPrice S(double p) { return LimitOrderBook::scale(p); }
}

TEST_CASE("LOB - empty by default", "[lob]") {
    LimitOrderBook b(42);
    REQUIRE(b.empty());
    double px; LimitOrderBook::AggQty q;
    REQUIRE_FALSE(b.best_bid(px, q));
    REQUIRE_FALSE(b.best_ask(px, q));
    REQUIRE(b.instrument_id() == 42u);
}

TEST_CASE("LOB - add bid then best_bid returns it", "[lob]") {
    LimitOrderBook b;
    b.apply_add('B', S(100.5), 7);
    double px; LimitOrderBook::AggQty q;
    REQUIRE(b.best_bid(px, q));
    REQUIRE(px == Catch::Approx(100.5));
    REQUIRE(q == 7u);
    REQUIRE_FALSE(b.best_ask(px, q));
}

TEST_CASE("LOB - bids descending, asks ascending", "[lob]") {
    LimitOrderBook b;
    b.apply_add('B', S(100.0), 1);
    b.apply_add('B', S(101.0), 2);
    b.apply_add('B', S(99.0),  3);
    b.apply_add('A', S(102.0), 5);
    b.apply_add('A', S(103.0), 6);

    double px; LimitOrderBook::AggQty q;
    b.best_bid(px, q);
    REQUIRE(px == Catch::Approx(101.0));
    REQUIRE(q == 2u);
    b.best_ask(px, q);
    REQUIRE(px == Catch::Approx(102.0));
    REQUIRE(q == 5u);
}

TEST_CASE("LOB - aggregation at same price", "[lob]") {
    LimitOrderBook b;
    b.apply_add('B', S(100.0), 5);
    b.apply_add('B', S(100.0), 3);
    REQUIRE(b.volume_at('B', S(100.0)) == 8u);
}

TEST_CASE("LOB - cancel decrements then removes level", "[lob]") {
    LimitOrderBook b;
    b.apply_add('B', S(100.0), 10);
    b.apply_cancel('B', S(100.0), 4);
    REQUIRE(b.volume_at('B', S(100.0)) == 6u);
    b.apply_cancel('B', S(100.0), 6);
    REQUIRE(b.volume_at('B', S(100.0)) == 0u);
    REQUIRE(b.bid_levels() == 0u);
}

TEST_CASE("LOB - cancel over-quantity removes level cleanly", "[lob]") {
    LimitOrderBook b;
    b.apply_add('A', S(50.0), 3);
    b.apply_cancel('A', S(50.0), 99);
    REQUIRE(b.empty());
}

TEST_CASE("LOB - fill behaves like cancel", "[lob]") {
    LimitOrderBook b;
    b.apply_add('A', S(200.25), 10);
    b.apply_fill('A', S(200.25), 4);
    REQUIRE(b.volume_at('A', S(200.25)) == 6u);
    b.apply_fill('A', S(200.25), 6);
    REQUIRE(b.empty());
}

TEST_CASE("LOB - clear wipes both sides", "[lob]") {
    LimitOrderBook b;
    b.apply_add('B', S(99.0),  1);
    b.apply_add('A', S(101.0), 1);
    b.clear();
    REQUIRE(b.empty());
}

TEST_CASE("LOB - snapshot top-N truncates", "[lob]") {
    LimitOrderBook b;
    for (int i = 0; i < 5; ++i) b.apply_add('B', S(100.0 - i), 1u + i);
    LimitOrderBook::Level out[3];
    auto n = b.snapshot_bids(out);
    REQUIRE(n == 3u);
    REQUIRE(out[0].price == Catch::Approx(100.0));
    REQUIRE(out[1].price == Catch::Approx(99.0));
    REQUIRE(out[2].price == Catch::Approx(98.0));
}
