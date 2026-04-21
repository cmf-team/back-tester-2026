#include "catch2/catch_all.hpp"
#include "common/MarketDataEvent.hpp"
#include "order_book/ShardedOrderBookRouter.hpp"
#include "order_book/SimpleOrderBookRouter.hpp"

#include <sstream>
#include <string>

using namespace cmf;

// Type aliases to avoid template comma issues in TEMPLATE_TEST_CASE
using SimpleRouter = SimpleOrderBookRouter<>;
using ShardedRouter2 = ShardedOrderBookRouter<MapOrderBook, 2>;
using ShardedRouter4 = ShardedOrderBookRouter<MapOrderBook, 4>;

// ---- helpers ---------------------------------------------------------------

static MarketDataEvent make_add(uint64_t order_id, uint32_t instrument_id,
                                Side side, int64_t price, uint32_t size) {
  MarketDataEvent e{};
  e.order_id = order_id;
  e.instrument_id = instrument_id;
  e.action = Action::Add;
  e.side = side;
  e.price = price;
  e.size = size;
  return e;
}

static MarketDataEvent make_cancel(uint64_t order_id, uint32_t instrument_id,
                                   uint32_t remaining_size) {
  MarketDataEvent e{};
  e.order_id = order_id;
  e.instrument_id = instrument_id;
  e.action = Action::Cancel;
  e.size = remaining_size;
  return e;
}

static MarketDataEvent make_modify(uint64_t order_id, uint32_t instrument_id,
                                   Side side, int64_t price, uint32_t size) {
  MarketDataEvent e{};
  e.order_id = order_id;
  e.instrument_id = instrument_id;
  e.action = Action::Modify;
  e.side = side;
  e.price = price;
  e.size = size;
  return e;
}

// ---- test cases ------------------------------------------------------------

TEMPLATE_TEST_CASE("OrderBookRouter - basic routing by instrument_id",
                   "[OrderBookRouter]", SimpleRouter, ShardedRouter2,
                   ShardedRouter4) {
  TestType router;

  constexpr int64_t P1 = 5000'000'000'000LL;
  constexpr int64_t P2 = 5010'000'000'000LL;

  auto e1 = make_add(1, 10, Side::Buy, P1, 100);
  router.apply(e1);
  auto e2 = make_add(2, 20, Side::Sell, P2, 50);
  router.apply(e2);

  std::ostringstream oss;
  router.print_snapshot(oss, 1);
  const std::string out = oss.str();

  REQUIRE(out.find("// 10") != std::string::npos);
  REQUIRE(out.find("// 20") != std::string::npos);
  REQUIRE(out.find("5000000000000") != std::string::npos);
  REQUIRE(out.find("5010000000000") != std::string::npos);
}

TEMPLATE_TEST_CASE(
    "OrderBookRouter - cancel with instrument_id==0 resolved via order_id",
    "[OrderBookRouter]", SimpleRouter, ShardedRouter2, ShardedRouter4) {
  TestType router;

  constexpr int64_t P = 5000'000'000'000LL;
  auto e1 = make_add(42, 10, Side::Buy, P, 100);
  router.apply(e1);
  auto e2 = make_cancel(42, /*instrument_id=*/0, /*remaining=*/0);
  router.apply(e2);

  std::ostringstream oss;
  router.print_snapshot(oss, 1);
  const std::string out = oss.str();

  REQUIRE(out.find("// 0") == std::string::npos);
  REQUIRE(out.empty());
}

TEMPLATE_TEST_CASE(
    "OrderBookRouter - modify with instrument_id==0 resolved via order_id",
    "[OrderBookRouter]", SimpleRouter, ShardedRouter2, ShardedRouter4) {
  TestType router;

  constexpr int64_t P_OLD = 5000'000'000'000LL;
  constexpr int64_t P_NEW = 4990'000'000'000LL;

  auto e1 = make_add(7, 10, Side::Buy, P_OLD, 100);
  router.apply(e1);
  auto e2 = make_modify(7, /*instrument_id=*/0, Side::Buy, P_NEW, 80);
  router.apply(e2);

  std::ostringstream oss;
  router.print_snapshot(oss, 1);
  const std::string out = oss.str();

  REQUIRE(out.find("4990000000000") != std::string::npos);
  REQUIRE(out.find("// 0") == std::string::npos);
}

TEMPLATE_TEST_CASE("OrderBookRouter - unresolvable order_id throws exception",
                   "[OrderBookRouter]", SimpleRouter, ShardedRouter2,
                   ShardedRouter4) {
  TestType router;

  MarketDataEvent bad_cancel{};
  bad_cancel.order_id = 999;
  bad_cancel.instrument_id = 0;
  bad_cancel.action = Action::Cancel;
  bad_cancel.size = 0;

  REQUIRE_THROWS_AS(router.apply(bad_cancel), std::runtime_error);
}

TEMPLATE_TEST_CASE(
    "OrderBookRouter - cache populated when instrument_id present",
    "[OrderBookRouter]", SimpleRouter, ShardedRouter2, ShardedRouter4) {
  TestType router;

  constexpr int64_t P = 5000'000'000'000LL;
  auto e1 = make_add(55, 15, Side::Buy, P, 100);
  router.apply(e1);

  auto e2 = make_modify(55, /*instrument_id=*/0, Side::Buy, P + 1e9, 50);
  router.apply(e2);

  std::ostringstream oss;
  router.print_snapshot(oss, 1);
  const std::string out = oss.str();

  REQUIRE(out.find("// 15") != std::string::npos);
  REQUIRE(out.find("// 0") == std::string::npos);
  REQUIRE(out.find("5001000000000") != std::string::npos);
}

TEMPLATE_TEST_CASE("OrderBookRouter - close idempotency", "[OrderBookRouter]",
                   SimpleRouter, ShardedRouter2, ShardedRouter4) {
  TestType router;

  constexpr int64_t P = 5000'000'000'000LL;
  auto e1 = make_add(1, 10, Side::Buy, P, 100);
  router.apply(e1);

  // First close should succeed
  if constexpr (std::is_same_v<TestType, SimpleOrderBookRouter<>>) {
    // SimpleOrderBookRouter doesn't have a close method
  } else {
    router.close();
    // Second close should not crash
    router.close();
  }

  std::ostringstream oss;
  router.print_snapshot(oss, 1);
  REQUIRE(!oss.str().empty());
}
