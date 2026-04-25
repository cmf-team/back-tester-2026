// Tests for InstrumentBookRegistry's routing behaviour, including the
// order_id -> instrument_id cache used when Cancel/Modify/Fill events
// arrive without an instrument_id.

#include "lob/InstrumentBookRegistry.hpp"
#include "parser/MarketDataEvent.hpp"

#include <catch2/catch_test_macros.hpp>

using cmf::Action;
using cmf::InstrumentBookRegistry;
using cmf::MarketDataEvent;
using cmf::MdSide;

namespace {

MarketDataEvent makeEv(cmf::OrderId oid, cmf::InstrumentId iid,
                       MdSide side, double price, double size,
                       Action action, cmf::NanoTime ts = 1) {
  MarketDataEvent ev;
  ev.timestamp     = ts;
  ev.order_id      = oid;
  ev.instrument_id = iid;
  ev.side          = side;
  ev.price         = price;
  ev.size          = size;
  ev.action        = action;
  return ev;
}

} // namespace

TEST_CASE("Registry: separate books per instrument", "[registry]") {
  InstrumentBookRegistry reg;
  reg.apply(makeEv(1, 100, MdSide::Bid, 50.0, 10, Action::Add));
  reg.apply(makeEv(2, 200, MdSide::Bid, 60.0, 20, Action::Add));

  REQUIRE(reg.size() == 2);
  REQUIRE(reg.find(100)->bestBidPrice() == 50.0);
  REQUIRE(reg.find(200)->bestBidPrice() == 60.0);
  REQUIRE(reg.find(300) == nullptr);
}

TEST_CASE("Registry: Cancel without iid resolves through cache",
          "[registry]") {
  InstrumentBookRegistry reg;
  reg.apply(makeEv(1, 100, MdSide::Bid, 50.0, 10, Action::Add));

  auto cancel_no_iid =
      makeEv(1, /*iid=*/0, MdSide::None, 50.0, 0, Action::Cancel);
  auto outcome = reg.apply(cancel_no_iid);
  REQUIRE(outcome == InstrumentBookRegistry::RouteResult::Applied);
  REQUIRE(reg.find(100)->openOrders() == 0);
}

TEST_CASE("Registry: cache evicts after full cancel", "[registry]") {
  InstrumentBookRegistry reg;
  reg.apply(makeEv(1, 100, MdSide::Bid, 50.0, 10, Action::Add));
  reg.apply(makeEv(1, 0,   MdSide::None, 50.0, 0, Action::Cancel));
  REQUIRE(reg.orderCacheSize() == 0);

  // A second Cancel for the same order_id is now an unknown order.
  auto outcome = reg.apply(
      makeEv(1, 0, MdSide::None, 50.0, 0, Action::Cancel));
  REQUIRE(outcome == InstrumentBookRegistry::RouteResult::UnknownOrder);
}

TEST_CASE("Registry: Clear without iid resets every book", "[registry]") {
  InstrumentBookRegistry reg;
  reg.apply(makeEv(1, 10, MdSide::Bid, 100.0, 5, Action::Add));
  reg.apply(makeEv(2, 20, MdSide::Ask, 200.0, 3, Action::Add));

  MarketDataEvent clear; clear.action = Action::Clear; clear.timestamp = 9;
  reg.apply(clear);
  REQUIRE(reg.find(10)->openOrders() == 0);
  REQUIRE(reg.find(20)->openOrders() == 0);
}
