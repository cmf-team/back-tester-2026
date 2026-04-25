// End-to-end smoke test for PipelineApp. Writes a tiny NDJSON file with
// two instruments, runs the full pipeline (producer + flat merger +
// dispatcher + snapshotter) and asserts event accounting.

#include "main2/PipelineApp.hpp"
#include "TempFile.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <sstream>

namespace {

const char *kNdjson =
"{\"ts_event\":\"2025-04-01T09:00:00.000000001Z\",\"order_id\":\"1\",\"side\":\"B\",\"price\":\"100.0\",\"size\":10,\"action\":\"A\",\"instrument_id\":100}\n"
"{\"ts_event\":\"2025-04-01T09:00:00.000000002Z\",\"order_id\":\"2\",\"side\":\"A\",\"price\":\"100.5\",\"size\":5,\"action\":\"A\",\"instrument_id\":100}\n"
"{\"ts_event\":\"2025-04-01T09:00:00.000000003Z\",\"order_id\":\"3\",\"side\":\"B\",\"price\":\"50.0\",\"size\":7,\"action\":\"A\",\"instrument_id\":200}\n"
"{\"ts_event\":\"2025-04-01T09:00:00.000000004Z\",\"order_id\":\"1\",\"side\":\"N\",\"price\":\"100.0\",\"size\":3,\"action\":\"C\",\"instrument_id\":100}\n";

} // namespace

TEST_CASE("PipelineApp: end-to-end NDJSON ingestion (sequential, flat)",
          "[pipeline][e2e]") {
  cmf::TempFile tmp("hw2_pipeline_smoke.json");
  {
    std::ofstream out(tmp.getPath());
    out << kNdjson;
  }

  cmf::PipelineConfig cfg;
  cfg.input          = tmp.getPath();
  cfg.merger         = cmf::MergerKind::Flat;
  cfg.workers        = 0;
  cfg.snapshot_every = 0;
  cfg.quiet          = true;

  std::ostringstream report, snaps;
  cmf::PipelineApp app(std::move(cfg));
  auto rpt = app.run(report, snaps);

  REQUIRE(rpt.stats.events_in    == 4);
  REQUIRE(rpt.stats.events_routed == 4);
  REQUIRE(rpt.instruments         == 2);
  REQUIRE(rpt.malformed_total     == 0);
}

TEST_CASE("PipelineApp: hierarchy merger gives identical counts",
          "[pipeline][e2e][equivalence]") {
  cmf::TempFile tmp("hw2_pipeline_smoke2.json");
  {
    std::ofstream out(tmp.getPath());
    out << kNdjson;
  }

  cmf::PipelineConfig cfg;
  cfg.input          = tmp.getPath();
  cfg.merger         = cmf::MergerKind::Hierarchy;
  cfg.workers        = 0;
  cfg.snapshot_every = 0;
  cfg.quiet          = true;

  std::ostringstream report, snaps;
  cmf::PipelineApp app(std::move(cfg));
  auto rpt = app.run(report, snaps);
  REQUIRE(rpt.stats.events_in == 4);
  REQUIRE(rpt.instruments     == 2);
}
