// Integration tests for MmapMboSource on temporary NDJSON files.

#include "market_data/MmapMboSource.hpp"

#include "TempFile.hpp"
#include "catch2/catch_all.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

using namespace cmf;

namespace {

// Real Clear + Add + Cancel lines from XEUR-20260409-HTT6HHLT6R.
constexpr const char *kLines[] = {
    R"({"ts_recv":"2026-04-06T18:53:08.486368500Z","hd":{"ts_event":"2026-04-06T18:53:08.486361336Z","rtype":160,"publisher_id":101,"instrument_id":453},"action":"R","side":"N","price":null,"size":0,"channel_id":23,"order_id":"0","flags":128,"ts_in_delta":7164,"sequence":0,"symbol":"FCEU SI 20281218 PS"})",
    R"({"ts_recv":"2026-04-07T00:00:00.246103535Z","hd":{"ts_event":"2026-04-07T00:00:00.246086711Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"1.157500000","size":20,"channel_id":23,"order_id":"10998892037100869125","flags":128,"ts_in_delta":985,"sequence":81255,"symbol":"FCEU SI 20260615 PS"})",
    R"({"ts_recv":"2026-04-07T00:00:02.399364973Z","hd":{"ts_event":"2026-04-07T00:00:02.399352523Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"C","side":"B","price":"1.157500000","size":20,"channel_id":23,"order_id":"10998892037100869125","flags":128,"ts_in_delta":882,"sequence":81351,"symbol":"FCEU SI 20260615 PS"})",
};

void writeFile(const std::filesystem::path &p, const std::string &body) {
  std::ofstream out(p, std::ios::binary);
  REQUIRE(out.is_open());
  out.write(body.data(), static_cast<std::streamsize>(body.size()));
}

} // namespace

TEST_CASE("MmapMboSource - streams three records in order", "[MmapMboSource]") {
  TempFile tf("mmap_mbo_source_3.mbo.json");
  std::string body;
  for (const char *l : kLines) {
    body.append(l);
    body.push_back('\n');
  }
  writeFile(tf.getPath(), body);

  MmapMboSource src(tf.getPath());
  MarketDataEvent e;

  REQUIRE(src.next(e));
  REQUIRE(e.action == MdAction::Clear);
  REQUIRE(e.instrument_id == 453);

  REQUIRE(src.next(e));
  REQUIRE(e.action == MdAction::Add);
  REQUIRE(e.order_id == 10998892037100869125ULL);

  REQUIRE(src.next(e));
  REQUIRE(e.action == MdAction::Cancel);

  REQUIRE_FALSE(src.next(e));
}

TEST_CASE("MmapMboSource - empty file yields zero events", "[MmapMboSource]") {
  TempFile tf("mmap_mbo_source_empty.mbo.json");
  writeFile(tf.getPath(), "");

  MmapMboSource src(tf.getPath());
  MarketDataEvent e;
  REQUIRE_FALSE(src.next(e));
}

TEST_CASE("MmapMboSource - file without trailing newline still fully parsed",
          "[MmapMboSource]") {
  TempFile tf("mmap_mbo_source_no_nl.mbo.json");
  // No trailing '\n'.
  writeFile(tf.getPath(), kLines[0]);

  MmapMboSource src(tf.getPath());
  MarketDataEvent e;
  REQUIRE(src.next(e));
  REQUIRE(e.action == MdAction::Clear);
  REQUIRE_FALSE(src.next(e));
}

TEST_CASE("MmapMboSource - rejects file with unexpected prefix",
          "[MmapMboSource]") {
  TempFile tf("mmap_mbo_source_bad_prefix.mbo.json");
  writeFile(tf.getPath(), "this is not a Databento MBO file\n");

  REQUIRE_THROWS_AS(MmapMboSource(tf.getPath()), std::runtime_error);
}
