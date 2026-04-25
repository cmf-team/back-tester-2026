// Tests for LobHandler — routing, dispatch, action filtering.

#include "lob/LobHandler.hpp"
#include "parser/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

#include <sstream>

using namespace cmf;

namespace {

constexpr int64_t kP100 = 100LL * MarketDataEvent::kPriceScale;

MarketDataEvent makeEv(Action a, uint32_t inst, OrderId id, MdSide side,
                       int64_t price, uint32_t size) {
  MarketDataEvent ev;
  ev.action        = a;
  ev.side          = side;
  ev.instrument_id = inst;
  ev.order_id      = id;
  ev.price         = price;
  ev.size          = size;
  return ev;
}

}  // namespace

TEST_CASE("LobHandler routes by instrument_id", "[LobHandler]") {
  LobHandler h;
  h.onEvent(makeEv(Action::Add, 100, 1, MdSide::Bid, kP100, 10));
  h.onEvent(makeEv(Action::Add, 200, 2, MdSide::Ask, kP100,  5));

  REQUIRE(h.bookCount() == 2u);
  REQUIRE(h.book(100) != nullptr);
  REQUIRE(h.book(200) != nullptr);
  REQUIRE(h.book(100)->bestBidVolume() == 10u);
  REQUIRE(h.book(200)->bestAskVolume() == 5u);
  REQUIRE(h.book(300) == nullptr);
}

TEST_CASE("LobHandler: Trade / Fill / None don't mutate the book",
          "[LobHandler]") {
  LobHandler h;
  h.onEvent(makeEv(Action::Add,   100, 1, MdSide::Bid,  kP100, 10));
  h.onEvent(makeEv(Action::Trade, 100, 0, MdSide::None, kP100,  1));
  h.onEvent(makeEv(Action::Fill,  100, 0, MdSide::None, kP100,  1));
  h.onEvent(makeEv(Action::None,  100, 0, MdSide::None, kP100,  1));

  REQUIRE(h.book(100)->bestBidVolume() == 10u);
  REQUIRE(h.book(100)->orderCount() == 1u);
}

TEST_CASE("LobHandler: Clear affects only its instrument", "[LobHandler]") {
  LobHandler h;
  h.onEvent(makeEv(Action::Add,   100, 1, MdSide::Bid, kP100, 10));
  h.onEvent(makeEv(Action::Add,   200, 2, MdSide::Bid, kP100,  5));
  h.onEvent(makeEv(Action::Clear, 100, 0, MdSide::None,
                   MarketDataEvent::kUndefPrice, 0));

  REQUIRE_FALSE(h.book(100)->hasBid());
  REQUIRE(h.book(200)->bestBidVolume() == 5u);
}

TEST_CASE("LobHandler reset drops all books", "[LobHandler]") {
  LobHandler h;
  h.onEvent(makeEv(Action::Add, 100, 1, MdSide::Bid, kP100, 10));
  h.onEvent(makeEv(Action::Add, 200, 2, MdSide::Ask, kP100,  5));
  REQUIRE(h.bookCount() == 2u);

  h.reset();
  REQUIRE(h.bookCount() == 0u);
  REQUIRE(h.book(100) == nullptr);
}

TEST_CASE("LobHandler partial vs full cancel via Cancel action",
          "[LobHandler]") {
  LobHandler h;
  h.onEvent(makeEv(Action::Add,    100, 1, MdSide::Bid, kP100, 10));

  // Partial: subtract 3 from 10.
  h.onEvent(makeEv(Action::Cancel, 100, 1, MdSide::Bid, kP100, 3));
  REQUIRE(h.book(100)->bestBidVolume() == 7u);

  // Full: subtract remaining 7.
  h.onEvent(makeEv(Action::Cancel, 100, 1, MdSide::Bid, kP100, 7));
  REQUIRE_FALSE(h.book(100)->hasBid());
}

TEST_CASE("LobHandler over-cancel clamps and warns once per instrument",
          "[LobHandler]") {
  LobHandler h;
  h.onEvent(makeEv(Action::Add,    100, 1, MdSide::Bid, kP100, 5));
  h.onEvent(makeEv(Action::Cancel, 100, 1, MdSide::Bid, kP100, 999));

  REQUIRE_FALSE(h.book(100)->hasBid());
  REQUIRE(h.book(100)->underflowWarnings() == 1u);
}

TEST_CASE("LobHandler printBestBidAsk: one line per instrument, sorted",
          "[LobHandler]") {
  LobHandler h;
  // Use prices with trailing zeros so auto-detected ticks are sane.
  h.onEvent(makeEv(Action::Add, 200, 1, MdSide::Bid, kP100, 10));
  h.onEvent(makeEv(Action::Add, 100, 2, MdSide::Bid, kP100,  5));
  h.onEvent(makeEv(Action::Add, 100, 3, MdSide::Ask,
                   kP100 + 10'000'000LL, 7));  // $100.01
  // 300 has only an ask, no bid.
  h.onEvent(makeEv(Action::Add, 300, 4, MdSide::Ask,
                   kP100 + 50'000'000LL, 1));  // $100.05

  std::ostringstream os;
  h.printBestBidAsk(os);
  const auto s = os.str();

  // Lines exist for all three instruments.
  REQUIRE(s.find("inst=100") != std::string::npos);
  REQUIRE(s.find("inst=200") != std::string::npos);
  REQUIRE(s.find("inst=300") != std::string::npos);

  // Sorted by id ascending.
  REQUIRE(s.find("inst=100") < s.find("inst=200"));
  REQUIRE(s.find("inst=200") < s.find("inst=300"));

  // Empty side rendered as "---".
  const auto pos300 = s.find("inst=300");
  const auto pos301 = s.find('\n', pos300);
  const auto line300 = s.substr(pos300, pos301 - pos300);
  REQUIRE(line300.find("bid=---") != std::string::npos);
  REQUIRE(line300.find("ask=100.050000000") != std::string::npos);
}

TEST_CASE("LobHandler per-instrument config overrides default", "[LobHandler]") {
  LobHandler h;
  LobConfig  cfg;
  cfg.tick_size = 50'000'000LL;  // 0.05
  h.setConfig(100, cfg);

  h.onEvent(makeEv(Action::Add, 100, 1, MdSide::Bid, kP100, 1));

  REQUIRE(h.book(100) != nullptr);
  REQUIRE(h.book(100)->config().tick_size == 50'000'000LL);
}
