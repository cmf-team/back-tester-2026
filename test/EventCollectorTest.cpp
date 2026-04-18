// tests for EventCollector

#include "main/EventCollector.hpp"
#include "parser/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

using namespace cmf;

namespace {

MarketDataEvent makeEvent(NanoTime ts, OrderId id) {
  MarketDataEvent ev;
  ev.timestamp = ts;
  ev.order_id = id;
  ev.action = Action::Add;
  ev.side = MdSide::Bid;
  ev.size = 1.0;
  ev.price = 1.0;
  return ev;
}

} // namespace

TEST_CASE("EventCollector keeps first N and last N", "[EventCollector]") {
  EventCollector c(3, 3);

  for (std::uint64_t i = 1; i <= 10; ++i)
    c(makeEvent(static_cast<NanoTime>(i), i));

  REQUIRE(c.total() == 10u);
  REQUIRE(c.firstTimestamp() == 1);
  REQUIRE(c.lastTimestamp() == 10);

  REQUIRE(c.firstEvents().size() == 3u);
  REQUIRE(c.firstEvents().front().order_id == 1u);
  REQUIRE(c.firstEvents().back().order_id == 3u);

  REQUIRE(c.lastEvents().size() == 3u);
  REQUIRE(c.lastEvents().front().order_id == 8u);
  REQUIRE(c.lastEvents().back().order_id == 10u);
}

TEST_CASE("EventCollector tolerates fewer events than the window",
          "[EventCollector]") {
  EventCollector c(10, 10);
  c(makeEvent(42, 7));

  REQUIRE(c.total() == 1u);
  REQUIRE(c.firstEvents().size() == 1u);
  REQUIRE(c.lastEvents().size() == 1u);
  REQUIRE(c.firstTimestamp() == 42);
  REQUIRE(c.lastTimestamp() == 42);
}

TEST_CASE("EventCollector::reset clears everything", "[EventCollector]") {
  EventCollector c(2, 2);
  c(makeEvent(1, 1));
  c(makeEvent(2, 2));
  c.reset();

  REQUIRE(c.total() == 0u);
  REQUIRE(c.firstEvents().empty());
  REQUIRE(c.lastEvents().empty());
}
