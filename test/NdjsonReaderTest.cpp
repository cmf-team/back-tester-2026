#include "ingest/NdjsonReader.hpp"
#include "TempFile.hpp"
#include "common/MarketDataEvent.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <vector>

using namespace cmf;

namespace {

void writeLines(const std::filesystem::path &p,
                const std::vector<std::string> &lines) {
  std::ofstream os(p);
  for (const auto &line : lines) {
    os << line << '\n';
  }
}

} // namespace

TEST_CASE("parseNdjsonFile ingests MBO lines and skips non-MBO / malformed",
          "[ingest]") {
  TempFile tmp("ndjson_reader_test.ndjson");
  writeLines(
      tmp.getPath(),
      {
          R"({"ts_recv":"2026-03-10T00:03:00.166748445Z","hd":{"ts_event":"2026-03-10T00:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":642004},"action":"R","side":"N","price":null,"size":0,"channel_id":23,"order_id":"0","flags":8,"ts_in_delta":0,"sequence":0})",
          R"({"ts_recv":"2026-03-10T00:03:00.166748445Z","hd":{"ts_event":"2026-03-10T00:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":642004},"action":"A","side":"B","price":"-0.005250000","size":10,"channel_id":23,"order_id":"10996472836897837169","flags":0,"ts_in_delta":2453,"sequence":29622})",
          R"({"ts_recv":"2026-03-10T00:36:29.559291401Z","hd":{"ts_event":"2026-03-10T00:36:29.559245905Z","rtype":160,"publisher_id":101,"instrument_id":642004},"action":"A","side":"A","price":"-0.004870000","size":10,"channel_id":23,"order_id":"1773102989559258459","flags":128,"ts_in_delta":1083,"sequence":37599})",
          R"({"ts_recv":"2026-03-10T00:36:29.559291401Z","hd":{"ts_event":"2026-03-10T00:36:29.559245905Z","rtype":1,"publisher_id":101,"instrument_id":642004},"action":"A","side":"A","price":"-0.004870000","size":10,"channel_id":23,"order_id":"1773102989559258459","flags":128,"ts_in_delta":1083,"sequence":37600})",
          R"(this is not json at all)",
      });

  std::vector<MarketDataEvent> seen;
  const auto stats = parseNdjsonFile(
      tmp.getPath(), [&](const MarketDataEvent &e) { seen.push_back(e); });

  REQUIRE(stats.consumed == 3);
  REQUIRE(stats.skipped_rtype == 1);
  REQUIRE(stats.skipped_parse == 1);
  REQUIRE(stats.first_ts_recv == 1'773'100'980'166'748'445ULL);
  REQUIRE(stats.last_ts_recv == 1'773'102'989'559'291'401ULL);
  // Informational: deliberately ordered input, counter should be zero.
  REQUIRE(stats.out_of_order_ts_recv == 0);

  REQUIRE(seen.size() == 3);
  REQUIRE(seen[0].order_id == 0);
  REQUIRE(seen[0].price == UNDEF_PRICE);
  REQUIRE(seen[0].action == MdAction::Clear);
  REQUIRE(seen[0].side == MdSide::None);
  REQUIRE(seen[1].order_id == 10'996'472'836'897'837'169ULL);
  REQUIRE(seen[1].action == MdAction::Add);
  REQUIRE(seen[1].side == MdSide::Bid);
  REQUIRE(seen[1].price == -5'250'000LL);
  REQUIRE(seen[2].action == MdAction::Add);
  REQUIRE(seen[2].price == -4'870'000LL);
  REQUIRE(seen[2].publisher_id == 101);
  REQUIRE(seen[2].instrument_id == 642'004U);
}

TEST_CASE("parseNdjsonFile accepts legacy top-level integer fields",
          "[ingest]") {
  TempFile tmp("ndjson_reader_rtype_str.ndjson");
  writeLines(
      tmp.getPath(),
      {
          R"({"rtype":"mbo","ts_recv":500,"ts_event":499,"publisher_id":2,"instrument_id":7,"order_id":"1","action":"A","side":"A","price":1000000000,"size":1,"sequence":1})",
      });

  MarketDataEvent event{};
  std::size_t count = 0;
  const auto stats =
      parseNdjsonFile(tmp.getPath(), [&](const MarketDataEvent &e) {
        ++count;
        event = e;
      });

  REQUIRE(stats.consumed == 1);
  REQUIRE(count == 1);
  REQUIRE(stats.skipped_rtype == 0);
  REQUIRE(stats.skipped_parse == 0);
  REQUIRE(event.ts_recv == 500);
  REQUIRE(event.ts_event == 499);
  REQUIRE(event.price == 1'000'000'000LL);
  REQUIRE(event.publisher_id == 2);
}

TEST_CASE("parseNdjsonFile counts out-of-order ts_recv without failing",
          "[ingest]") {
  TempFile tmp("ndjson_reader_ooo.ndjson");
  writeLines(
      tmp.getPath(),
      {
          R"({"ts_recv":"2026-03-10T00:00:00.000000001Z","hd":{"ts_event":"2026-03-10T00:00:00.000000000Z","rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"1","action":"A","side":"B","price":"0.000000000","size":1,"sequence":1})",
          R"({"ts_recv":"2026-03-10T00:00:00.000000000Z","hd":{"ts_event":"2026-03-10T00:00:00.000000000Z","rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"2","action":"A","side":"B","price":"0.000000000","size":1,"sequence":2})",
          R"({"ts_recv":"2026-03-10T00:00:00.000000002Z","hd":{"ts_event":"2026-03-10T00:00:00.000000000Z","rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"3","action":"A","side":"B","price":"0.000000000","size":1,"sequence":3})",
      });

  const auto stats =
      parseNdjsonFile(tmp.getPath(), [](const MarketDataEvent &) {});

  REQUIRE(stats.consumed == 3);
  REQUIRE(stats.out_of_order_ts_recv == 1);
  REQUIRE(stats.first_ts_recv == 1'773'100'800'000'000'001ULL);
  REQUIRE(stats.last_ts_recv == 1'773'100'800'000'000'002ULL);
}
