// tests for MarketDataEvent + FileMarketDataSource

#include "parser/FileMarketDataSource.hpp"
#include "parser/MarketDataEvent.hpp"

#include "TempFile.hpp"

#include "catch2/catch_all.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

using namespace cmf;

namespace {

constexpr const char *kSampleClear =
    R"({"ts_recv":"2026-04-06T18:53:11.430340287Z","hd":{"ts_event":"2026-04-06T18:53:11.430338519Z","rtype":160,"publisher_id":101,"instrument_id":33974},"action":"R","side":"N","price":null,"size":0,"channel_id":79,"order_id":"0","flags":128,"ts_in_delta":1768,"sequence":0,"symbol":"EUCO SI 20270312 PS EU P 1.2250 0"})";

constexpr const char *kSampleAdd =
    R"({"ts_recv":"2026-04-07T07:00:00.123456789Z","hd":{"ts_event":"2026-04-07T07:00:00.123456000Z","rtype":160,"publisher_id":101,"instrument_id":42},"action":"A","side":"B","price":"1.156100000","size":7,"channel_id":12,"order_id":"123456789","flags":0,"ts_in_delta":-5,"sequence":42,"symbol":"FOO BAR"})";

} // namespace

TEST_CASE("ISO timestamp parsing", "[MarketDataEvent]") {
  REQUIRE(parseIso8601Nanos("1970-01-01T00:00:00.000000000Z") == 0);
  REQUIRE(parseIso8601Nanos("1970-01-01T00:00:00.000000001Z") == 1);
  // 2026-04-07T07:00:00Z = 1'775'545'200 s since epoch.
  REQUIRE(parseIso8601Nanos("2026-04-07T07:00:00.000000000Z") ==
          1'775'545'200'000'000'000LL);
  // round trip
  const auto ns = parseIso8601Nanos("2026-04-07T07:00:00.123456789Z");
  REQUIRE(formatIso8601Nanos(ns) == "2026-04-07T07:00:00.123456789Z");
}

TEST_CASE("parseMarketDataLine - clear (null price)", "[MarketDataEvent]") {
  MarketDataEvent ev;
  REQUIRE(parseMarketDataLine(kSampleClear, ev) == ParseStatus::Ok);

  REQUIRE(ev.action == Action::Clear);
  REQUIRE(ev.side == MdSide::None);
  REQUIRE(ev.order_id == 0u);
  REQUIRE(ev.size == 0.0);
  REQUIRE_FALSE(priceDefined(ev.price));
  REQUIRE(ev.timestamp == parseIso8601Nanos("2026-04-06T18:53:11.430338519Z"));
  REQUIRE(ev.ts_recv == parseIso8601Nanos("2026-04-06T18:53:11.430340287Z"));
  REQUIRE(ev.instrument_id == 33974u);
  REQUIRE(ev.publisher_id == 101u);
  REQUIRE(ev.channel_id == 79u);
  REQUIRE(ev.flags == 128u);
  REQUIRE(ev.ts_in_delta == 1768);
  REQUIRE(ev.sequence == 0u);
  REQUIRE(ev.symbol == "EUCO SI 20270312 PS EU P 1.2250 0");
}

TEST_CASE("parseMarketDataLine - add (real price + side)", "[MarketDataEvent]") {
  MarketDataEvent ev;
  REQUIRE(parseMarketDataLine(kSampleAdd, ev) == ParseStatus::Ok);

  REQUIRE(ev.action == Action::Add);
  REQUIRE(ev.side == MdSide::Bid);
  REQUIRE(ev.order_id == 123456789ULL);
  REQUIRE(ev.size == 7.0);
  REQUIRE(priceDefined(ev.price));
  REQUIRE(std::abs(ev.price - 1.1561) < 1e-12);
  REQUIRE(ev.ts_in_delta == -5);
  REQUIRE(ev.sequence == 42u);
  REQUIRE(ev.symbol == "FOO BAR");
}

TEST_CASE("parseMarketDataLine - rejects malformed input", "[MarketDataEvent]") {
  MarketDataEvent ev;
  REQUIRE(parseMarketDataLine("", ev) == ParseStatus::Empty);
  REQUIRE(parseMarketDataLine("not-json", ev) == ParseStatus::Malformed);
  REQUIRE(parseMarketDataLine("{}", ev) == ParseStatus::Malformed);
}

TEST_CASE("MarketDataEvent stream insertion", "[MarketDataEvent]") {
  MarketDataEvent ev;
  REQUIRE(parseMarketDataLine(kSampleAdd, ev) == ParseStatus::Ok);

  std::ostringstream os;
  os << ev;
  const std::string s = os.str();
  REQUIRE(s.find("order_id=123456789") != std::string::npos);
  REQUIRE(s.find("side=B") != std::string::npos);
  REQUIRE(s.find("action=A") != std::string::npos);
  REQUIRE(s.find("size=7") != std::string::npos);
  REQUIRE(s.find("symbol=\"FOO BAR\"") != std::string::npos);
}

TEST_CASE("FileMarketDataSource - reads NDJSON file", "[MarketDataEvent]") {
  TempFile tmp("md_test.ndjson");
  {
    std::ofstream of(tmp.getPath());
    of << kSampleClear << "\n";
    of << "\n";          // empty line - skipped
    of << "garbage\n";   // malformed - skipped
    of << kSampleAdd << "\n";
  }

  FileMarketDataSource src(tmp.getPath());

  MarketDataEvent ev;
  REQUIRE(src.next(ev));
  REQUIRE(ev.action == Action::Clear);

  REQUIRE(src.next(ev));
  REQUIRE(ev.action == Action::Add);
  REQUIRE(ev.order_id == 123456789ULL);

  REQUIRE_FALSE(src.next(ev));

  REQUIRE(src.skippedCount() == 1u);
  REQUIRE(src.malformedCount() == 1u);
}
