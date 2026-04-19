#include "common/MarketDataEvent.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

using namespace cmf;

TEST_CASE("MarketDataEvent defaults carry sentinels", "[mde]") {
  MarketDataEvent e;
  REQUIRE(e.ts_recv == UNDEF_TIMESTAMP);
  REQUIRE(e.ts_event == UNDEF_TIMESTAMP);
  REQUIRE(e.price == UNDEF_PRICE);
  REQUIRE(e.size == 0);
  REQUIRE(e.action == MdAction::None);
  REQUIRE(e.side == MdSide::None);
}

TEST_CASE(
    "MarketDataEvent comparator orders by ts_recv, publisher_id, sequence",
    "[mde]") {
  MarketDataEvent a{};
  a.ts_recv = 100;
  a.publisher_id = 1;
  a.sequence = 10;

  MarketDataEvent b = a;
  b.ts_recv = 101;
  REQUIRE(a < b);

  MarketDataEvent c = a;
  c.publisher_id = 2;
  REQUIRE(a < c);
  REQUIRE(c < b); // ts_recv dominates

  MarketDataEvent d = a;
  d.sequence = 11;
  REQUIRE(a < d);
}

TEST_CASE("MarketDataEvent exposes order-key equivalence explicitly", "[mde]") {
  MarketDataEvent a{};
  a.ts_recv = 42;
  a.publisher_id = 3;
  a.sequence = 7;
  // Differ in fields outside the key — sameOrderKey should treat as equal.
  MarketDataEvent b = a;
  b.order_id = 99999;
  b.price = 123;
  b.size = 5;
  REQUIRE(sameOrderKey(a, b));
  REQUIRE_FALSE(a < b);
  REQUIRE_FALSE(b < a);

  MarketDataEvent c = a;
  c.sequence = 8;
  REQUIRE_FALSE(sameOrderKey(a, c));
}

TEST_CASE("MarketDataEvent price preserves int64 fixed precision", "[mde]") {
  MarketDataEvent e{};
  e.ts_recv = 1;
  e.order_id = 42;
  e.price = 5'411'750'000'000LL; // 5411.75 per Databento documentation
  e.size = 3;
  e.action = MdAction::Add;
  e.side = MdSide::Bid;

  std::ostringstream os;
  os << e;
  const std::string out = os.str();
  REQUIRE(out.find("price=5411.750000000") != std::string::npos);
  REQUIRE(out.find("order_id=42") != std::string::npos);
  REQUIRE(out.find("action=A") != std::string::npos);
  REQUIRE(out.find("side=B") != std::string::npos);
  REQUIRE(out.find("size=3") != std::string::npos);
  REQUIRE(out.find("ts_recv=1") != std::string::npos);
}

TEST_CASE("MarketDataEvent undef price renders as null", "[mde]") {
  MarketDataEvent e{};
  std::ostringstream os;
  os << e;
  REQUIRE(os.str().find("price=null") != std::string::npos);
}
