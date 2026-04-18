// tests for FileMarketDataSource

#include "parser/FileMarketDataSource.hpp"
#include "TempFile.hpp"

#include "catch2/catch_all.hpp"

#include <fstream>
#include <string>

using namespace cmf;

namespace {

// One Add from the example set (options Add).
constexpr const char* kAddLine =
    R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"0.021200000","size":20,"channel_id":79,"order_id":"10996414798222631105","flags":0,"ts_in_delta":2365,"sequence":52012,"symbol":"EUCO SI 20260710 PS EU P 1.1650 0"})"
    "\n";

// Clear record with price:null (tests the UNDEF path).
constexpr const char* kClearLine =
    R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"R","side":"N","price":null,"size":0,"channel_id":79,"order_id":"0","flags":8,"ts_in_delta":0,"sequence":0,"symbol":"EUCO SI 20260710 PS EU P 1.1650 0"})"
    "\n";

// Futures Trade with side=A (seller aggressor) — negative-free aggressor case.
constexpr const char* kTradeLine =
    R"({"ts_recv":"2026-03-09T07:45:01.735130301Z","hd":{"ts_event":"2026-03-09T07:45:01.731911761Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"T","side":"A","price":"1.156100000","size":18,"channel_id":23,"order_id":"1773042301735051569","flags":0,"ts_in_delta":1304,"sequence":1570115,"symbol":"FCEU SI 20260316 PS"})"
    "\n";

} // namespace

TEST_CASE("MarketDataParser parses an Add line", "[MarketDataParser]") {
  const std::string buf = kAddLine;
  FileMarketDataSource  p(buf.data(), buf.data() + buf.size());

  MarketDataEvent ev;
  REQUIRE(p.next(ev));

  REQUIRE(ev.rtype == 160);
  REQUIRE(ev.publisher_id == 101);
  REQUIRE(ev.instrument_id == 34513);
  REQUIRE(ev.action == Action::Add);
  REQUIRE(ev.side == MdSide::Bid);
  REQUIRE(ev.price == 21'200'000LL);          // 0.021200000 * 1e9
  REQUIRE(ev.priceDefined());
  REQUIRE(ev.priceAsDouble() == Catch::Approx(0.0212));
  REQUIRE(ev.size == 20u);
  REQUIRE(ev.channel_id == 79u);
  REQUIRE(ev.order_id == 10996414798222631105ULL);
  REQUIRE(ev.flags == 0u);
  REQUIRE(ev.ts_in_delta == 2365);
  REQUIRE(ev.sequence == 52012u);
  REQUIRE(ev.symbol == "EUCO SI 20260710 PS EU P 1.1650 0");

  // ts_recv - ts_event = 368'148'840 - 367'824'437 = 324'403 ns
  REQUIRE(ev.ts_recv - ev.ts_event == 324'403);

  // Expected absolute ts_recv: 2026-03-09T07:52:41.368148840Z
  // = 1'773'042'761 s + 0.368148840 s
  REQUIRE(ev.ts_recv == 1'773'042'761'368'148'840LL);

  // Single-line buffer: next call must report EOF.
  REQUIRE_FALSE(p.next(ev));
}

TEST_CASE("MarketDataParser handles price=null on Clear", "[MarketDataParser]") {
  const std::string buf = kClearLine;
  FileMarketDataSource  p(buf.data(), buf.data() + buf.size());

  MarketDataEvent ev;
  REQUIRE(p.next(ev));
  REQUIRE(ev.action == Action::Clear);
  REQUIRE(ev.side == MdSide::None);
  REQUIRE_FALSE(ev.priceDefined());
  REQUIRE(ev.price == MarketDataEvent::kUndefPrice);
  REQUIRE(ev.order_id == 0ULL);
  REQUIRE(ev.size == 0u);
  REQUIRE(ev.flags == 8u);                     // F_BAD_TS_RECV
  REQUIRE(ev.sequence == 0u);
  REQUIRE(ev.ts_in_delta == 0);
  REQUIRE(ev.symbol == "EUCO SI 20260710 PS EU P 1.1650 0");
}

TEST_CASE("MarketDataParser parses multiple lines in order",
          "[MarketDataParser]") {
  const std::string buf = std::string(kClearLine) + kAddLine + kTradeLine;
  FileMarketDataSource  p(buf.data(), buf.data() + buf.size());

  MarketDataEvent ev;

  REQUIRE(p.next(ev));
  REQUIRE(ev.action == Action::Clear);
  REQUIRE(ev.instrument_id == 34513);

  REQUIRE(p.next(ev));
  REQUIRE(ev.action == Action::Add);
  REQUIRE(ev.instrument_id == 34513);
  REQUIRE(ev.size == 20u);

  REQUIRE(p.next(ev));
  REQUIRE(ev.action == Action::Trade);
  REQUIRE(ev.side == MdSide::Ask);
  REQUIRE(ev.instrument_id == 442);
  REQUIRE(ev.price == 1'156'100'000LL);        // 1.156100000 * 1e9
  REQUIRE(ev.size == 18u);
  REQUIRE(ev.symbol == "FCEU SI 20260316 PS");

  REQUIRE_FALSE(p.next(ev));
  REQUIRE(p.bytesConsumed() == p.totalBytes());
}

TEST_CASE("MarketDataParser mmap round-trip via a real file",
          "[MarketDataParser][mmap]") {
  TempFile      tf("mdp_mmap_test.jsonl");
  {
    std::ofstream os(tf.getPath());
    os << kClearLine << kAddLine << kTradeLine;
  }

  FileMarketDataSource p(tf.getPath());
  MarketDataEvent  ev;

  int lines = 0;
  while (p.next(ev)) ++lines;
  REQUIRE(lines == 3);
}
