// Snapshotter: tests focus on the captured payload (since the writer
// thread just pushes formatted bytes to an ostream).

#include "lob/InstrumentBookRegistry.hpp"
#include "parser/MarketDataEvent.hpp"
#include "pipeline/Snapshotter.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <thread>
#include <chrono>

using cmf::Action;
using cmf::InstrumentBookRegistry;
using cmf::MarketDataEvent;
using cmf::MdSide;
using cmf::Snapshotter;

namespace {

MarketDataEvent mk(cmf::OrderId oid, cmf::InstrumentId iid, MdSide s,
                   double px, double sz, cmf::NanoTime ts) {
  MarketDataEvent e;
  e.timestamp     = ts;
  e.order_id      = oid;
  e.instrument_id = iid;
  e.side          = s;
  e.price         = px;
  e.size          = sz;
  e.action        = Action::Add;
  return e;
}

} // namespace

TEST_CASE("Snapshotter::capture mirrors registry state at call time",
          "[snapshotter]") {
  InstrumentBookRegistry reg;
  reg.apply(mk(1, 100, MdSide::Bid, 50.0, 10, 1));
  reg.apply(mk(2, 100, MdSide::Ask, 51.0, 5,  2));
  reg.apply(mk(3, 200, MdSide::Bid, 25.0, 7,  3));

  std::ostringstream sink;
  Snapshotter snap(reg, sink, /*depth=*/3);
  auto s = snap.capture(/*event_seq=*/3, /*last_ts=*/3);
  REQUIRE(s.event_seq   == 3);
  REQUIRE(s.last_ts     == 3);
  REQUIRE(s.instruments == 2);
  REQUIRE(s.per_instrument.size() == 2);
}

TEST_CASE("Snapshotter writes asynchronously and stops cleanly",
          "[snapshotter][threads]") {
  InstrumentBookRegistry reg;
  reg.apply(mk(1, 100, MdSide::Bid, 50.0, 10, 1));

  std::ostringstream sink;
  Snapshotter snap(reg, sink, /*depth=*/3);
  snap.start();
  for (int i = 0; i < 5; ++i)
    snap.captureAndSubmit(/*seq=*/i + 1, /*ts=*/i + 1);
  snap.stop();
  REQUIRE(snap.emitted() == 5);
  REQUIRE(sink.str().find("[snapshot @seq=1") != std::string::npos);
  REQUIRE(sink.str().find("[snapshot @seq=5") != std::string::npos);
}
