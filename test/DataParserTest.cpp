#include "TempFile.hpp"
#include "catch2/catch_all.hpp"
#include "ingestion/NativeDataParser.hpp"
#include "ingestion/SimpleDataParser.hpp"

#include <cmath>
#include <fstream>
#include <string>
#include <vector>

using namespace cmf;

// 2026-04-01T00:00:00Z = Unix second 1775001600
static constexpr int64_t BASE_SEC = 1775001600LL;
static constexpr int64_t NS = 1'000'000'000LL;

TEMPLATE_TEST_CASE("DataParser - parses 3 JSON lines", "[DataParser]",
                   SimpleDataParser, NativeDataParser) {
  TempFile tmp("dataparser_test.mbo.json");
  {
    std::ofstream fs(tmp.getPath());
    fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"A","price":"1.161100000","size":20,"channel_id":23,"order_id":"1775001621480460739","flags":128,"ts_in_delta":1041,"sequence":120067,"symbol":"FCEU SI 20260615 PS"})"
       << "\n";
    fs << R"({"ts_recv":"2026-04-01T00:00:21.480541935Z","hd":{"ts_event":"2026-04-01T00:00:21.480529509Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"C","side":"A","price":"1.161200000","size":20,"channel_id":23,"order_id":"1775001620423626831","flags":128,"ts_in_delta":1056,"sequence":120070,"symbol":"FCEU SI 20260615 PS"})"
       << "\n";
    fs << R"({"ts_recv":"2026-04-01T00:00:23.720604803Z","hd":{"ts_event":"2026-04-01T00:00:23.720590379Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"C","side":"B","price":"1.160700000","size":20,"channel_id":23,"order_id":"10998373657385341895","flags":128,"ts_in_delta":1538,"sequence":120283,"symbol":"FCEU SI 20260615 PS"})"
       << "\n";
  }

  std::vector<MarketDataEvent> events;
  TestType parser(tmp.getPath());
  parser.parse([&](const MarketDataEvent &e) { events.push_back(e); });

  REQUIRE(events.size() == 3);

  // Event 0: action A, side A
  {
    const auto &e = events[0];
    REQUIRE(e.ts_recv == (BASE_SEC + 21) * NS + 480'470'598LL);
    REQUIRE(e.ts_event == (BASE_SEC + 21) * NS + 480'454'469LL);
    REQUIRE(e.action == Action::Add);
    REQUIRE(e.side == static_cast<Side>('A'));
    REQUIRE(e.order_id == 1775001621480460739ULL);
    REQUIRE(e.instrument_id == 436);
    REQUIRE(e.publisher_id == 101);
    REQUIRE(e.size == 20);
    REQUIRE(e.channel_id == 23);
    REQUIRE(e.rtype == RType::Mbo);
    REQUIRE(e.flags == Flags::Last);
    REQUIRE(e.ts_in_delta == 1041);
    REQUIRE(e.sequence == 120067);
    REQUIRE(e.is_price_defined());
    REQUIRE(e.price_as_double() == Catch::Approx(1.1611).epsilon(1e-4));
  }

  // Event 1: action C, side A
  {
    const auto &e = events[1];
    REQUIRE(e.ts_recv == (BASE_SEC + 21) * NS + 480'541'935LL);
    REQUIRE(e.action == Action::Cancel);
    REQUIRE(e.side == static_cast<Side>('A'));
    REQUIRE(e.order_id == 1775001620423626831ULL);
    REQUIRE(e.sequence == 120070);
  }

  // Event 2: action C, side B
  {
    const auto &e = events[2];
    REQUIRE(e.ts_recv == (BASE_SEC + 23) * NS + 720'604'803LL);
    REQUIRE(e.ts_event == (BASE_SEC + 23) * NS + 720'590'379LL);
    REQUIRE(e.action == Action::Cancel);
    REQUIRE(e.side == static_cast<Side>('B'));
    REQUIRE(e.order_id == 10998373657385341895ULL);
    REQUIRE(e.ts_in_delta == 1538);
    REQUIRE(e.sequence == 120283);
  }
}

TEMPLATE_TEST_CASE("DataParser - skips malformed lines", "[DataParser]",
                   SimpleDataParser, NativeDataParser) {
  TempFile tmp("dataparser_bad.mbo.json");
  {
    std::ofstream fs(tmp.getPath());
    fs << "not json at all\n";
    fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"A","price":"1.161100000","size":20,"channel_id":23,"order_id":"1775001621480460739","flags":128,"ts_in_delta":1041,"sequence":120067,"symbol":"X"})"
       << "\n";
  }

  std::vector<MarketDataEvent> events;
  TestType parser(tmp.getPath());
  parser.parse([&](const MarketDataEvent &e) { events.push_back(e); });

  REQUIRE(events.size() == 1);
}

TEMPLATE_TEST_CASE("DataParser - empty file sets empty flag", "[DataParser]",
                   SimpleDataParser, NativeDataParser) {
  TempFile tmp("dataparser_empty.mbo.json");
  {
    std::ofstream fs(tmp.getPath());
  }

  std::vector<MarketDataEvent> events;
  TestType parser(tmp.getPath());
  parser.parse([&](const MarketDataEvent &e) { events.push_back(e); });

  REQUIRE(events.empty());
}

TEMPLATE_TEST_CASE("DataParser - parses negative price", "[DataParser]",
                   SimpleDataParser, NativeDataParser) {
  TempFile tmp("dataparser_negprice.mbo.json");
  {
    std::ofstream fs(tmp.getPath());
    fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"-0.250000000","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":1,"symbol":"OPT"})"
       << "\n";
    fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"-5.000000000","size":1,"channel_id":1,"order_id":"2","flags":0,"ts_in_delta":0,"sequence":2,"symbol":"OPT"})"
       << "\n";
  }

  std::vector<MarketDataEvent> events;
  TestType parser(tmp.getPath());
  parser.parse([&](const MarketDataEvent &e) { events.push_back(e); });

  REQUIRE(events.size() == 2);
  REQUIRE(events[0].price == -250'000'000LL);   // -0.25 * 1e9
  REQUIRE(events[1].price == -5'000'000'000LL); // -5.0 * 1e9
}
