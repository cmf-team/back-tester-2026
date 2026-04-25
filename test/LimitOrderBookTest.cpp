// Unit tests for cmf::LimitOrderBook.
//
// We intentionally test BBO and per-action behaviour with hand-crafted
// MarketDataEvent values rather than going through JSON parsing -- it
// makes the failure modes self-contained.

#include "lob/LimitOrderBook.hpp"
#include "parser/MarketDataEvent.hpp"

#include <catch2/catch_test_macros.hpp>

using cmf::Action;
using cmf::LimitOrderBook;
using cmf::MarketDataEvent;
using cmf::MdSide;

namespace {

MarketDataEvent makeEv(cmf::OrderId oid, MdSide side, double price,
                       double size, Action action,
                       cmf::NanoTime ts = 1) {
  MarketDataEvent ev;
  ev.timestamp = ts;
  ev.order_id  = oid;
  ev.side      = side;
  ev.price     = price;
  ev.size      = size;
  ev.action    = action;
  return ev;
}

} // namespace

TEST_CASE("LimitOrderBook: empty BBO is unset", "[lob]") {
  LimitOrderBook lob;
  REQUIRE_FALSE(lob.hasBid());
  REQUIRE_FALSE(lob.hasAsk());
  REQUIRE(lob.bidLevels() == 0);
  REQUIRE(lob.askLevels() == 0);
}

TEST_CASE("LimitOrderBook: Add builds BBO", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 100.0, 10, Action::Add));
  lob.apply(makeEv(2, MdSide::Bid, 99.5,  20, Action::Add));
  lob.apply(makeEv(3, MdSide::Ask, 100.5, 5,  Action::Add));
  lob.apply(makeEv(4, MdSide::Ask, 101.0, 7,  Action::Add));

  REQUIRE(lob.bestBidPrice() == 100.0);
  REQUIRE(lob.bestAskPrice() == 100.5);
  REQUIRE(lob.bestBidQty()   == 10);
  REQUIRE(lob.bestAskQty()   == 5);
  REQUIRE(lob.bidLevels() == 2);
  REQUIRE(lob.askLevels() == 2);
  REQUIRE(lob.openOrders() == 4);
  REQUIRE(lob.addCount() == 4);
}

TEST_CASE("LimitOrderBook: Cancel partial reduces, full removes", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 100.0, 10, Action::Add));
  lob.apply(makeEv(2, MdSide::Bid, 100.0, 5,  Action::Add)); // same level, total 15

  // Partial cancel -3 of order 1
  auto cancel_partial = makeEv(1, MdSide::None, 100.0, 3, Action::Cancel);
  lob.apply(cancel_partial);
  REQUIRE(lob.bestBidQty() == 12);
  REQUIRE(lob.openOrders() == 2);

  // Full cancel of order 2 (size==0)
  auto cancel_full = makeEv(2, MdSide::None, 100.0, 0, Action::Cancel);
  lob.apply(cancel_full);
  REQUIRE(lob.bestBidQty() == 7);
  REQUIRE(lob.openOrders() == 1);
}

TEST_CASE("LimitOrderBook: Modify on same price adjusts qty", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Ask, 50.0, 10, Action::Add));
  lob.apply(makeEv(1, MdSide::None, 50.0, 14, Action::Modify));
  REQUIRE(lob.bestAskQty() == 14);
  REQUIRE(lob.askLevels() == 1);
}

TEST_CASE("LimitOrderBook: Modify across price moves the order", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 50.0, 10, Action::Add));
  lob.apply(makeEv(1, MdSide::None, 49.5, 10, Action::Modify));
  REQUIRE(lob.bestBidPrice() == 49.5);
  REQUIRE(lob.bidLevels() == 1);
}

TEST_CASE("LimitOrderBook: Fill behaves like partial cancel", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 100.0, 10, Action::Add));
  lob.apply(makeEv(1, MdSide::None, 100.0, 4, Action::Fill));
  REQUIRE(lob.bestBidQty() == 6);
  REQUIRE(lob.fillCount() == 1);
}

TEST_CASE("LimitOrderBook: Trade does NOT mutate the book", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 100.0, 10, Action::Add));
  auto trade = makeEv(0, MdSide::None, 100.0, 5, Action::Trade);
  lob.apply(trade);
  REQUIRE(lob.bestBidQty() == 10);
  REQUIRE(lob.tradeCount() == 1);
}

TEST_CASE("LimitOrderBook: Clear wipes both sides", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 100.0, 10, Action::Add));
  lob.apply(makeEv(2, MdSide::Ask, 101.0, 8,  Action::Add));
  MarketDataEvent clear; clear.action = Action::Clear; clear.timestamp = 100;
  lob.apply(clear);
  REQUIRE_FALSE(lob.hasBid());
  REQUIRE_FALSE(lob.hasAsk());
  REQUIRE(lob.openOrders() == 0);
  REQUIRE(lob.clearCount() == 1);
}

TEST_CASE("LimitOrderBook: top-N is best-first", "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 100.0, 1, Action::Add));
  lob.apply(makeEv(2, MdSide::Bid, 99.5,  2, Action::Add));
  lob.apply(makeEv(3, MdSide::Bid, 99.0,  3, Action::Add));
  lob.apply(makeEv(4, MdSide::Bid, 98.5,  4, Action::Add));

  auto top = lob.topBids(3);
  REQUIRE(top.size() == 3);
  REQUIRE(top[0].first == 100.0);
  REQUIRE(top[1].first == 99.5);
  REQUIRE(top[2].first == 99.0);
}

TEST_CASE("LimitOrderBook: tick scaling round-trips", "[lob][tick]") {
  // Verifies prices we routinely encounter survive tick conversion.
  for (double p : {1.23456789, 0.000000001, 12345.6789, 99999.0}) {
    auto t = LimitOrderBook::toTick(p);
    REQUIRE(LimitOrderBook::fromTick(t) == p);
  }
}

TEST_CASE("LimitOrderBook: volumeAtPrice aggregates across orders",
          "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(1, MdSide::Bid, 100.0, 10, Action::Add));
  lob.apply(makeEv(2, MdSide::Bid, 100.0, 5,  Action::Add));
  lob.apply(makeEv(3, MdSide::Bid,  99.5,  7, Action::Add));
  REQUIRE(lob.volumeAtPrice(MdSide::Bid, 100.0) == 15);
  REQUIRE(lob.volumeAtPrice(MdSide::Bid,  99.5) == 7);
  REQUIRE(lob.volumeAtPrice(MdSide::Bid,  98.0) == 0);
  REQUIRE(lob.volumeAtPrice(MdSide::Ask, 100.0) == 0);
}

TEST_CASE("LimitOrderBook: Cancel of unknown order is counted as skipped",
          "[lob]") {
  LimitOrderBook lob;
  lob.apply(makeEv(42, MdSide::None, 100.0, 5, Action::Cancel));
  REQUIRE(lob.skippedCount() == 1);
  REQUIRE(lob.openOrders() == 0);
}
