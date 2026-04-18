// tests for MarketDataParser — standard task
//
// Uses a TempFile to create a mock NDJSON file with a mix of:
//   - valid messages covering every action type (R, A, C, M, T, F)
//   - malformed / empty / incomplete lines that the parser must skip

#include "main/ingestion/MarketDataParser.hpp"

#include "TempFile.hpp"
#include "catch2/catch_all.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

using namespace cmf;
using Catch::Approx;

// ── Fixture data ────────────────────────────────────────────────────────
// Real Databento records (pretty_px / pretty_ts enabled) plus hand-crafted
// invalid lines.  Each valid line is taken from the actual XEUR EOBI feed
// or modelled closely on it so the field values are realistic.

// Line 1 — Clear (R), null price, side N
static constexpr const char *kClearLine =
    R"({"ts_recv":"2026-03-09T00:03:00.129732099Z","hd":{"ts_event":"2026-03-09T00:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"R","side":"N","price":null,"size":0,"channel_id":23,"order_id":"0","flags":8,"ts_in_delta":0,"sequence":0,"symbol":"FCEU SI 20260316 PS"})";

// Line 2 — Add (A), buy side
static constexpr const char *kAddBuyLine =
    R"({"ts_recv":"2026-03-09T00:03:00.129732099Z","hd":{"ts_event":"2026-03-09T00:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"A","side":"B","price":"1.152800000","size":20,"channel_id":23,"order_id":"10996386599372464268","flags":0,"ts_in_delta":2634,"sequence":61463,"symbol":"FCEU SI 20260316 PS"})";

// Line 3 — Add (A), ask/sell side, F_LAST flag (128)
static constexpr const char *kAddSellLine =
    R"({"ts_recv":"2026-03-09T00:03:00.129732099Z","hd":{"ts_event":"2026-03-09T00:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"A","side":"A","price":"1.153200000","size":20,"channel_id":23,"order_id":"1773014562391096283","flags":128,"ts_in_delta":2634,"sequence":61463,"symbol":"FCEU SI 20260316 PS"})";

// Line 4 — Cancel (C)
static constexpr const char *kCancelLine =
    R"({"ts_recv":"2026-03-09T00:03:01.430638368Z","hd":{"ts_event":"2026-03-09T00:03:01.430619997Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"C","side":"A","price":"1.153200000","size":20,"channel_id":23,"order_id":"1773014562391096283","flags":128,"ts_in_delta":1201,"sequence":61595,"symbol":"FCEU SI 20260316 PS"})";

// Line 5 — Trade (T), seller aggressor
static constexpr const char *kTradeLine =
    R"({"ts_recv":"2026-03-09T07:45:01.735130301Z","hd":{"ts_event":"2026-03-09T07:45:01.731911761Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"T","side":"A","price":"1.156100000","size":18,"channel_id":23,"order_id":"1773042301735051569","flags":0,"ts_in_delta":1304,"sequence":1570115,"symbol":"FCEU SI 20260316 PS"})";

// Line 6 — Fill (F), resting buy filled
static constexpr const char *kFillLine =
    R"({"ts_recv":"2026-03-09T07:45:01.735130301Z","hd":{"ts_event":"2026-03-09T07:45:01.731911761Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"F","side":"B","price":"1.156100000","size":18,"channel_id":23,"order_id":"10996414336265404691","flags":0,"ts_in_delta":1304,"sequence":1570115,"symbol":"FCEU SI 20260316 PS"})";

// Line 7 — Modify (M)
static constexpr const char *kModifyLine =
    R"({"ts_recv":"2026-03-09T13:51:44.824178477Z","hd":{"ts_event":"2026-03-09T13:51:44.824161033Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"M","side":"A","price":"1.158050000","size":20,"channel_id":23,"order_id":"1773064304823766933","flags":128,"ts_in_delta":1374,"sequence":4205894,"symbol":"FCEU SI 20260316 PS"})";

// Line 8 — Options record (different instrument, small price)
static constexpr const char *kOptionsLine =
    R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"0.021200000","size":20,"channel_id":79,"order_id":"10996414798222631105","flags":0,"ts_in_delta":2365,"sequence":52012,"symbol":"EUCO SI 20260710 PS EU P 1.1650 0"})";

// Invalid lines
static constexpr const char *kEmptyLine = "";
static constexpr const char *kWhitespaceLine = "   ";
static constexpr const char *kNotJson = "this is not json at all";
static constexpr const char *kBrokenJson =
    R"({"ts_recv":"2026-03-09T00:03:00.129732099Z","hd":)";
static constexpr const char *kMissingAction =
    R"({"ts_recv":"2026-03-09T00:03:00.129732099Z","hd":{"ts_event":"2026-03-09T00:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":442},"side":"B","price":"1.152800000","size":20,"channel_id":23,"order_id":"123","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"TEST"})";

// ── Helper ──────────────────────────────────────────────────────────────
// Writes lines into a temp file, one per line (NDJSON).
static void writeMockFile(const std::filesystem::path &path,
                          const std::vector<const char *> &lines) {
  std::ofstream ofs(path);
  for (const auto *line : lines) {
    ofs << line << "\n";
  }
}

// ── parseLine tests ─────────────────────────────────────────────────────

TEST_CASE("parseLine — Clear action with null price", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kClearLine, evt));

  CHECK(evt.action == Action::Clear);
  CHECK(evt.side == Side::None);
  CHECK(evt.size == Approx(0.0));
  CHECK(evt.orderId == 0);
  CHECK(evt.instrumentId == 442);
  CHECK(evt.channelId == 23);
  CHECK(evt.publisherId == 101);
  CHECK(evt.rtype == 160);
  CHECK(evt.flags == 8);
  CHECK(evt.tsInDelta == 0);
  CHECK(evt.sequence == 0);
  CHECK(evt.symbol == "FCEU SI 20260316 PS");
}

TEST_CASE("parseLine — Add buy", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kAddBuyLine, evt));

  CHECK(evt.action == Action::Add);
  CHECK(evt.side == Side::Buy);
  CHECK(evt.price == Approx(1.152800000));
  CHECK(evt.size == Approx(20.0));
  CHECK(evt.orderId == 10996386599372464268ULL);
  CHECK(evt.flags == 0);
  CHECK(evt.tsInDelta == 2634);
  CHECK(evt.sequence == 61463);
}

TEST_CASE("parseLine — Add sell (ask side)", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kAddSellLine, evt));

  CHECK(evt.action == Action::Add);
  CHECK(evt.side == Side::Sell);
  CHECK(evt.price == Approx(1.153200000));
  CHECK(evt.flags == 128);
  CHECK(evt.orderId == 1773014562391096283ULL);
}

TEST_CASE("parseLine — Cancel", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kCancelLine, evt));

  CHECK(evt.action == Action::Cancel);
  CHECK(evt.side == Side::Sell);
  CHECK(evt.price == Approx(1.153200000));
  CHECK(evt.sequence == 61595);
}

TEST_CASE("parseLine — Trade", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kTradeLine, evt));

  CHECK(evt.action == Action::Trade);
  CHECK(evt.side == Side::Sell); // aggressor was seller
  CHECK(evt.price == Approx(1.156100000));
  CHECK(evt.size == Approx(18.0));
}

TEST_CASE("parseLine — Fill", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kFillLine, evt));

  CHECK(evt.action == Action::Fill);
  CHECK(evt.side == Side::Buy); // resting buy was filled
  CHECK(evt.price == Approx(1.156100000));
  CHECK(evt.size == Approx(18.0));
}

TEST_CASE("parseLine — Modify", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kModifyLine, evt));

  CHECK(evt.action == Action::Modify);
  CHECK(evt.price == Approx(1.158050000));
  CHECK(evt.sequence == 4205894);
}

TEST_CASE("parseLine — Options record (small price)", "[MarketDataParser]") {
  MarketDataEvent evt;
  REQUIRE(MarketDataParser::parseLine(kOptionsLine, evt));

  CHECK(evt.price == Approx(0.021200000));
  CHECK(evt.instrumentId == 34513);
  CHECK(evt.channelId == 79);
  CHECK(evt.symbol == "EUCO SI 20260710 PS EU P 1.1650 0");
}

TEST_CASE("parseLine — rejects empty line", "[MarketDataParser]") {
  MarketDataEvent evt;
  CHECK_FALSE(MarketDataParser::parseLine(kEmptyLine, evt));
}

TEST_CASE("parseLine — rejects whitespace-only line", "[MarketDataParser]") {
  MarketDataEvent evt;
  CHECK_FALSE(MarketDataParser::parseLine(kWhitespaceLine, evt));
}

TEST_CASE("parseLine — rejects non-JSON", "[MarketDataParser]") {
  MarketDataEvent evt;
  CHECK_FALSE(MarketDataParser::parseLine(kNotJson, evt));
}

TEST_CASE("parseLine — rejects truncated JSON", "[MarketDataParser]") {
  MarketDataEvent evt;
  CHECK_FALSE(MarketDataParser::parseLine(kBrokenJson, evt));
}

TEST_CASE("parseLine — rejects JSON missing required field",
          "[MarketDataParser]") {
  MarketDataEvent evt;
  CHECK_FALSE(MarketDataParser::parseLine(kMissingAction, evt));
}

// ── parseFile tests ─────────────────────────────────────────────────────

TEST_CASE("parseFile — counts only valid lines", "[MarketDataParser]") {
  TempFile tmp("test_mixed.mbo.json");
  writeMockFile(tmp.getPath(), {
                                   kClearLine,   // valid  (1)
                                   kAddBuyLine,  // valid  (2)
                                   kEmptyLine,   // skip
                                   kNotJson,     // skip
                                   kTradeLine,   // valid  (3)
                                   kBrokenJson,  // skip
                                   kFillLine,    // valid  (4)
                                   kModifyLine,  // valid  (5)
                                   kOptionsLine, // valid  (6)
                                   kAddSellLine, // valid  (7)
                                   kCancelLine,  // valid  (8)
                               });

  MarketDataParser parser;
  std::vector<MarketDataEvent> events;
  std::size_t count =
      parser.parseFile(tmp.getPath().string(),
                       [&](const MarketDataEvent &e) { events.push_back(e); });

  CHECK(count == 8);
  CHECK(events.size() == 8);
}

TEST_CASE("parseFile — empty file yields zero events", "[MarketDataParser]") {
  TempFile tmp("test_empty.mbo.json");
  writeMockFile(tmp.getPath(), {});

  MarketDataParser parser;
  std::size_t count =
      parser.parseFile(tmp.getPath().string(), [](const MarketDataEvent &) {});

  CHECK(count == 0);
}

TEST_CASE("parseFile — all-invalid file yields zero events",
          "[MarketDataParser]") {
  TempFile tmp("test_all_invalid.mbo.json");
  writeMockFile(tmp.getPath(), {
                                   kEmptyLine,
                                   kNotJson,
                                   kBrokenJson,
                                   kWhitespaceLine,
                               });

  MarketDataParser parser;
  std::size_t count =
      parser.parseFile(tmp.getPath().string(), [](const MarketDataEvent &) {});

  CHECK(count == 0);
}

TEST_CASE("parseFile — events arrive in file order", "[MarketDataParser]") {
  TempFile tmp("test_order.mbo.json");
  // Clear comes first (ts 00:03:00), then Trade (ts 07:45:01)
  writeMockFile(tmp.getPath(), {kClearLine, kTradeLine});

  MarketDataParser parser;
  std::vector<Action> actions;
  parser.parseFile(tmp.getPath().string(), [&](const MarketDataEvent &e) {
    actions.push_back(e.action);
  });

  REQUIRE(actions.size() == 2);
  CHECK(actions[0] == Action::Clear);
  CHECK(actions[1] == Action::Trade);
}
