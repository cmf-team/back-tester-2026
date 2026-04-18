// tests for Statistics

#include "stats/Statistics.hpp"

#include "catch2/catch_all.hpp"

#include <sstream>

using namespace cmf;

namespace {

MarketDataEvent makeEvent(Action a, uint32_t inst, uint16_t pub, NanoTime ts,
                          int64_t price, uint8_t flags, std::string sym, int32_t ts_in_delta = 0) {
  MarketDataEvent ev;
  ev.ts_recv       = ts;
  ev.ts_event      = ts;
  ev.action        = a;
  ev.side          = MdSide::None;
  ev.instrument_id = inst;
  ev.publisher_id  = pub;
  ev.price         = price;
  ev.flags         = flags;
  ev.symbol        = std::move(sym);
  ev.ts_in_delta   = ts_in_delta;
  return ev;
}

} // namespace

TEST_CASE("Statistics aggregates global counters", "[Statistics]") {
  Statistics stats;

  stats.onEvent(makeEvent(Action::Add,   100, 101, 1'000, 500'000'000LL, 0, "AAA", 100));
  stats.onEvent(makeEvent(Action::Trade, 100, 101, 2'000, 501'000'000LL, 0, "AAA", 200));
  stats.onEvent(makeEvent(Action::Clear, 100, 101,   500,
                          MarketDataEvent::kUndefPrice, 0, "AAA"));

  REQUIRE(stats.totalEvents() == 3u);
  REQUIRE(stats.firstTs() == 500);      // min ts_recv
  REQUIRE(stats.lastTs()  == 2'000);    // max ts_recv

  REQUIRE(stats.actionCount(Action::Add)   == 1u);
  REQUIRE(stats.actionCount(Action::Trade) == 1u);
  REQUIRE(stats.actionCount(Action::Clear) == 1u);
  REQUIRE(stats.actionCount(Action::Cancel) == 0u);

  REQUIRE(stats.publisherCounts().at(101) == 3u);

  // Price range ignores the undef-price Clear.
  REQUIRE(stats.hasPrice());
  REQUIRE(stats.minPrice() == 500'000'000LL);
  REQUIRE(stats.maxPrice() == 501'000'000LL);

  REQUIRE(stats.tsAvgDeltaNs() == Catch::Approx(100.0));
}

TEST_CASE("Statistics per-instrument + symbol dictionary", "[Statistics]") {
  Statistics stats;

  stats.onEvent(makeEvent(Action::Add, 100, 101, 1'000, 100'000'000LL, 0, "AAA"));
  stats.onEvent(makeEvent(Action::Add, 100, 101, 2'000, 110'000'000LL, 0, "AAA"));
  stats.onEvent(makeEvent(Action::Add, 200, 101, 1'500,  50'000'000LL, 0, "BBB"));

  REQUIRE(stats.instrumentStats().size() == 2u);

  const auto& a = stats.instrumentStats().at(100);
  REQUIRE(a.count == 2u);
  REQUIRE(a.first_ts == 1'000);
  REQUIRE(a.last_ts  == 2'000);
  REQUIRE(a.min_price == 100'000'000LL);
  REQUIRE(a.max_price == 110'000'000LL);
  REQUIRE(a.symbol == "AAA");

  const auto& b = stats.instrumentStats().at(200);
  REQUIRE(b.count == 1u);
  REQUIRE(b.symbol == "BBB");
  REQUIRE(b.min_price == 50'000'000LL);
  REQUIRE(b.max_price == 50'000'000LL);
}

TEST_CASE("Statistics detects ts_recv regressions per instrument",
          "[Statistics]") {
  Statistics stats;

  stats.onEvent(makeEvent(Action::Add, 100, 101, 2'000, 100'000'000LL, 0, "AAA"));
  stats.onEvent(makeEvent(Action::Add, 100, 101, 1'000, 100'000'000LL, 0, "AAA")); // regression
  stats.onEvent(makeEvent(Action::Add, 100, 101, 1'500, 100'000'000LL, 0, "AAA")); // regression
  stats.onEvent(makeEvent(Action::Add, 200, 101, 1'000, 100'000'000LL, 0, "BBB")); // different instrument
  stats.onEvent(makeEvent(Action::Add, 200, 101,   500, 100'000'000LL, 0, "BBB")); // regression

  REQUIRE(stats.tsRecvRegressions() == 3u);
  REQUIRE(stats.instrumentStats().at(100).ts_recv_regressions == 2u);
  REQUIRE(stats.instrumentStats().at(200).ts_recv_regressions == 1u);
}

TEST_CASE("Statistics counts gap/quality flags", "[Statistics]") {
  Statistics stats;
  constexpr uint8_t kMaybeBadBook = 1u << 2;  // 0x04
  constexpr uint8_t kBadTsRecv    = 1u << 3;  // 0x08

  stats.onEvent(makeEvent(Action::Add, 100, 101, 1'000, 1, 0, "AAA"));
  stats.onEvent(makeEvent(Action::Add, 100, 101, 2'000, 1, kMaybeBadBook, "AAA"));
  stats.onEvent(makeEvent(Action::Add, 100, 101, 3'000, 1, kBadTsRecv, "AAA"));
  stats.onEvent(makeEvent(Action::Add, 100, 101, 4'000, 1,
                          kMaybeBadBook | kBadTsRecv, "AAA"));

  REQUIRE(stats.maybeBadBookEvents() == 2u);
  REQUIRE(stats.badTsRecvEvents()    == 2u);
}

TEST_CASE("Statistics operator<< renders a summary", "[Statistics]") {
  Statistics stats;

  // Use a timestamp we can verify: 2026-03-09T07:52:41.368148840Z
  constexpr NanoTime kTs = 1'773'042'761'368'148'840LL;
  stats.onEvent(makeEvent(Action::Add, 34513, 101, kTs, 21'200'000LL, 0,
                          "EUCO SI 20260710 PS EU P 1.1650 0"));

  std::ostringstream os;
  os << stats;
  const auto out = os.str();

  REQUIRE(out.find("total_events          = 1") != std::string::npos);
  REQUIRE(out.find("2026-03-09T07:52:41.368148840Z") != std::string::npos);
  REQUIRE(out.find("0.021200000") != std::string::npos);
  REQUIRE(out.find("EUCO SI 20260710 PS EU P 1.1650 0") != std::string::npos);
  REQUIRE(out.find("A = 1") != std::string::npos);
}

TEST_CASE("Statistics reset() clears all state", "[Statistics]") {
  Statistics stats;
  stats.onEvent(makeEvent(Action::Add, 100, 101, 1'000, 100'000'000LL, 0, "AAA"));
  REQUIRE(stats.totalEvents() == 1u);

  stats.reset();
  REQUIRE(stats.totalEvents() == 0u);
  REQUIRE_FALSE(stats.hasPrice());
  REQUIRE(stats.instrumentStats().empty());
  REQUIRE(stats.publisherCounts().empty());
  REQUIRE(stats.actionCount(Action::Add) == 0u);
}
