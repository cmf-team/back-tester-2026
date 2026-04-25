// PipelineApp: orchestrates a full HW-2 pipeline run.
//
// Responsibilities:
//   * resolve the input (single file or directory of NDJSON files),
//   * stand up one Producer per file,
//   * pick the merger (flat / hierarchy),
//   * choose between sequential Dispatcher and ShardedDispatcher,
//   * wire in Snapshotter,
//   * collect and print final stats.
//
// All policy-level decisions live in PipelineConfig; the class itself is
// tiny glue, which makes it the natural integration test point.

#pragma once

#include "lob/InstrumentBookRegistry.hpp"
#include "pipeline/Dispatcher.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace cmf {

enum class MergerKind { Flat, Hierarchy };

struct PipelineConfig {
  std::filesystem::path        input;        // file or directory
  MergerKind                   merger{MergerKind::Flat};
  std::size_t                  workers{0};   // 0 = sequential dispatcher
  std::uint64_t                snapshot_every{0}; // 0 = no snapshots
  std::size_t                  snapshot_depth{5};
  std::size_t                  queue_capacity{4096};
  bool                         quiet{false};

  // Resolves input into the list of NDJSON files to ingest. Sorted by
  // filename for reproducibility.
  std::vector<std::filesystem::path> resolveInputFiles() const;
};

struct InstrumentBbo {
  InstrumentId iid{0};
  bool         has_bid{false};
  double       bid_px{0.0};
  std::int64_t bid_qty{0};
  bool         has_ask{false};
  double       ask_px{0.0};
  std::int64_t ask_qty{0};
  std::size_t  bid_levels{0};
  std::size_t  ask_levels{0};
  std::size_t  open_orders{0};
};

struct PipelineReport {
  DispatcherStats stats;
  std::size_t     instruments{0};
  std::uint64_t   total_orders{0};
  std::uint64_t   produced_total{0};
  std::uint64_t   malformed_total{0};
  std::uint64_t   snapshots_emitted{0};

  // Performance --------------------------------------------------------
  // wall_seconds is measured around the dispatcher run only (parsing in
  // the producers is overlapped). events_per_sec is derived from
  // events_in / wall_seconds.
  double          wall_seconds{0.0};
  double          events_per_sec{0.0};

  // Final best bid/ask per instrument (HW-2 standard variant requirement).
  std::vector<InstrumentBbo> final_bbo;
};

class PipelineApp {
public:
  explicit PipelineApp(PipelineConfig cfg) : cfg_(std::move(cfg)) {}

  // Run end-to-end. Snapshots are written to `snapshot_out`; the final
  // report goes to `report_out`.
  PipelineReport run(std::ostream &report_out, std::ostream &snapshot_out);

private:
  PipelineConfig cfg_;
};

} // namespace cmf
