// Unit tests for MarketDataEvent: enum parsing, defaults, ordering, dump.

#include "market_data/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

#include <cmath>
#include <sstream>
#include <stdexcept>

using namespace cmf;

TEST_CASE("MarketDataEvent - parseAction maps all documented codes",
          "[MarketDataEvent]") {
  REQUIRE(parseAction('A') == MdAction::Add);
  REQUIRE(parseAction('M') == MdAction::Modify);
  REQUIRE(parseAction('C') == MdAction::Cancel);
  REQUIRE(parseAction('R') == MdAction::Clear);
  REQUIRE(parseAction('T') == MdAction::Trade);
  REQUIRE(parseAction('F') == MdAction::Fill);
  REQUIRE(parseAction('N') == MdAction::None);
  REQUIRE_THROWS_AS(parseAction('X'), std::invalid_argument);
  REQUIRE_THROWS_AS(parseAction('\0'), std::invalid_argument);
}

TEST_CASE("MarketDataEvent - parseSide maps all documented codes",
          "[MarketDataEvent]") {
  REQUIRE(parseSide('B') == Side::Buy);
  REQUIRE(parseSide('A') == Side::Sell);
  REQUIRE(parseSide('N') == Side::None);
  REQUIRE_THROWS_AS(parseSide('X'), std::invalid_argument);
}

TEST_CASE("MarketDataEvent - default values", "[MarketDataEvent]") {
  MarketDataEvent e;
  REQUIRE(e.ts_recv == 0);
  REQUIRE(e.ts_event == 0);
  REQUIRE(e.action == MdAction::None);
  REQUIRE(e.side == Side::None);
  REQUIRE(e.order_id == 0);
  REQUIRE(e.size == 0);
  REQUIRE(e.price == MarketDataEvent::kUndefPrice);
  REQUIRE_FALSE(e.priceDefined());
  REQUIRE(std::isnan(e.priceAsDouble()));
  REQUIRE(e.symbol.empty());
}

TEST_CASE("MarketDataEvent - fixed-point price round-trip",
          "[MarketDataEvent]") {
  MarketDataEvent e;
  e.price = 1'157'500'000LL; // "1.157500000"
  REQUIRE(e.priceDefined());
  REQUIRE(e.priceAsDouble() == Catch::Approx(1.1575).margin(1e-9));

  e.price = -42'000'000'000LL; // "-42.000000000"
  REQUIRE(e.priceAsDouble() == Catch::Approx(-42.0).margin(1e-9));
}

TEST_CASE("MarketDataEvent - ordering by ts_recv primary",
          "[MarketDataEvent]") {
  MarketDataEvent a, b;
  a.ts_recv = 100;
  b.ts_recv = 200;
  REQUIRE(a < b);
  REQUIRE_FALSE(b < a);
  REQUIRE(b > a);
  REQUIRE_FALSE(a > b);
}

TEST_CASE("MarketDataEvent - ordering tie-break by sequence then instrument",
          "[MarketDataEvent]") {
  MarketDataEvent a, b;
  a.ts_recv = b.ts_recv = 100;

  SECTION("sequence tie-break") {
    a.sequence = 1;
    b.sequence = 2;
    REQUIRE(a < b);
    REQUIRE(b > a);
  }

  SECTION("instrument_id tie-break when sequence equal") {
    a.sequence = b.sequence = 5;
    a.instrument_id = 1;
    b.instrument_id = 2;
    REQUIRE(a < b);
  }

  SECTION("fully equal is neither less nor greater") {
    REQUIRE_FALSE(a < b);
    REQUIRE_FALSE(b < a);
  }
}

TEST_CASE("MarketDataEvent - operator<< prints key fields",
          "[MarketDataEvent]") {
  MarketDataEvent e;
  e.ts_recv = 1775501588486368500LL;
  e.order_id = 42;
  e.side = Side::Buy;
  e.price = 1'234'500'000LL; // 1.2345 in fixed-point (1e-9 scale)
  e.size = 10;
  e.action = MdAction::Add;
  e.instrument_id = 453;
  e.symbol = "FCEU";

  std::ostringstream os;
  os << e;
  const std::string s = os.str();
  REQUIRE(s.find("ts_recv=1775501588486368500") != std::string::npos);
  REQUIRE(s.find("order_id=42") != std::string::npos);
  REQUIRE(s.find("action=A") != std::string::npos);
  REQUIRE(s.find("instrument_id=453") != std::string::npos);
  REQUIRE(s.find("symbol=FCEU") != std::string::npos);
}
