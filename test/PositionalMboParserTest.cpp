// Unit tests for PositionalMboParser: identical expectations as the (to be
// retired) MboJsonParser, driven by real captured lines.

#include "market_data/PositionalMboParser.hpp"

#include "catch2/catch_all.hpp"

#include <cstring>
#include <string>

using namespace cmf;

// Real line (action='R', price=null) from
// XEUR-20260409-HTT6HHLT6R/xeur-eobi-20260406.mbo.json, first record.
constexpr const char *kClearLine =
    R"JSON({"ts_recv":"2026-04-06T18:53:08.486368500Z","hd":{"ts_event":"2026-04-06T18:53:08.486361336Z","rtype":160,"publisher_id":101,"instrument_id":453},"action":"R","side":"N","price":null,"size":0,"channel_id":23,"order_id":"0","flags":128,"ts_in_delta":7164,"sequence":0,"symbol":"FCEU SI 20281218 PS"})JSON";

// Real line (action='A', side='B', non-null price string) from
// XEUR-20260409-HTT6HHLT6R/xeur-eobi-20260407.mbo.json.
constexpr const char *kAddLine =
    R"JSON({"ts_recv":"2026-04-07T00:00:00.246103535Z","hd":{"ts_event":"2026-04-07T00:00:00.246086711Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"1.157500000","size":20,"channel_id":23,"order_id":"10998892037100869125","flags":128,"ts_in_delta":985,"sequence":81255,"symbol":"FCEU SI 20260615 PS"})JSON";

constexpr const char *kCancelLine =
    R"JSON({"ts_recv":"2026-04-07T00:00:02.399364973Z","hd":{"ts_event":"2026-04-07T00:00:02.399352523Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"C","side":"B","price":"1.157500000","size":20,"channel_id":23,"order_id":"10998892037100869125","flags":128,"ts_in_delta":882,"sequence":81351,"symbol":"FCEU SI 20260615 PS"})JSON";

namespace {

MarketDataEvent parseOne(const char *line) {
  const std::size_t n = std::strlen(line);
  PositionalMboParser p(line, line + n);
  MarketDataEvent e;
  REQUIRE(p.next(e));
  return e;
}

} // namespace

TEST_CASE("PositionalMboParser - Clear (action=R, price=null)",
          "[PositionalMboParser]") {
  const auto e = parseOne(kClearLine);

  REQUIRE(e.ts_recv == 1775501588486368500LL);
  REQUIRE(e.ts_event == 1775501588486361336LL);
  REQUIRE(e.rtype == 160);
  REQUIRE(e.publisher_id == 101);
  REQUIRE(e.instrument_id == 453);
  REQUIRE(e.action == MdAction::Clear);
  REQUIRE(e.side == Side::None);
  REQUIRE_FALSE(e.priceDefined());
  REQUIRE(e.price == MarketDataEvent::kUndefPrice);
  REQUIRE(e.size == 0);
  REQUIRE(e.channel_id == 23);
  REQUIRE(e.order_id == 0);
  REQUIRE(e.flags == 128);
  REQUIRE(e.ts_in_delta == 7164);
  REQUIRE(e.sequence == 0);
  REQUIRE(e.symbol == "FCEU SI 20281218 PS");
}

TEST_CASE("PositionalMboParser - Add (real Buy-side order)",
          "[PositionalMboParser]") {
  const auto e = parseOne(kAddLine);

  REQUIRE(e.action == MdAction::Add);
  REQUIRE(e.side == Side::Buy);
  REQUIRE(e.price == 1'157'500'000LL);
  REQUIRE(e.priceAsDouble() == Catch::Approx(1.1575).margin(1e-9));
  REQUIRE(e.size == 20);
  REQUIRE(e.instrument_id == 436);
  REQUIRE(e.order_id == 10998892037100869125ULL);
  REQUIRE(e.sequence == 81255);
  REQUIRE(e.flags == 128);
  REQUIRE(e.symbol == "FCEU SI 20260615 PS");
}

TEST_CASE("PositionalMboParser - Cancel (action=C)", "[PositionalMboParser]") {
  const auto e = parseOne(kCancelLine);
  REQUIRE(e.action == MdAction::Cancel);
  REQUIRE(e.side == Side::Buy);
  REQUIRE(e.price == 1'157'500'000LL);
  REQUIRE(e.order_id == 10998892037100869125ULL);
}

TEST_CASE("PositionalMboParser - three records back-to-back in one buffer",
          "[PositionalMboParser]") {
  std::string buf;
  buf.append(kClearLine);
  buf.push_back('\n');
  buf.append(kAddLine);
  buf.push_back('\n');
  buf.append(kCancelLine);
  buf.push_back('\n');

  PositionalMboParser p(buf.data(), buf.data() + buf.size());
  MarketDataEvent e;

  REQUIRE(p.next(e));
  REQUIRE(e.action == MdAction::Clear);
  REQUIRE(p.next(e));
  REQUIRE(e.action == MdAction::Add);
  REQUIRE(p.next(e));
  REQUIRE(e.action == MdAction::Cancel);
  REQUIRE_FALSE(p.next(e));
}

TEST_CASE("PositionalMboParser - fixed-point price round-trips to double",
          "[PositionalMboParser]") {
  const auto e = parseOne(kAddLine);
  // "1.157500000" -> 1_157_500_000 -> 1.1575 within ~1e-9 double precision.
  REQUIRE(e.price == 1'157'500'000LL);
  REQUIRE(static_cast<double>(e.price) / 1e9 ==
          Catch::Approx(1.1575).margin(1e-9));
}

TEST_CASE("PositionalMboParser - empty buffer yields no events",
          "[PositionalMboParser]") {
  PositionalMboParser p(nullptr, nullptr);
  MarketDataEvent e;
  REQUIRE_FALSE(p.next(e));
}

TEST_CASE("PositionalMboParser - negative price parses to negative fixed-point",
          "[PositionalMboParser]") {
  // Synthetic line with a negative spread price. Same layout as real data.
  const std::string line =
      R"({"ts_recv":"2026-04-07T00:00:00.000000000Z","hd":{"ts_event":"2026-04-07T00:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"-1.500000000","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"X"})";

  const auto e = parseOne(line.c_str());
  REQUIRE(e.price == -1'500'000'000LL);
  REQUIRE(e.priceAsDouble() == Catch::Approx(-1.5).margin(1e-9));
}
