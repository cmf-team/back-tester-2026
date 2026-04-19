// Multi-file hard-variant ingest pipeline.
//
// One producer thread reads each file into a per-file queue. Strategy-specific
// merger threads combine those sorted streams into one final queue, which a
// single dispatcher thread drains in chronological order.

#pragma once

#include "common/MarketDataEvent.hpp"
#include "ingest/NdjsonReader.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace cmf {

enum class MergeStrategy {
  Flat,
  Hierarchy,
};

const char *mergeStrategyName(MergeStrategy strategy) noexcept;

// Defaults tuned against the 44-file real Eurex benchmark: 512-event blocks
// keep per-flush sync overhead low without blowing dispatcher latency, and a
// 64-block queue is enough to absorb producer jitter without unbounded memory.
// Zero is rejected (see ingestFolder) so downstream code can assume >= 1.
struct FolderIngestOptions {
  std::size_t queue_capacity = 64;
  std::size_t batch_size = 512;
};

struct FolderIngestStats {
  MergeStrategy strategy{MergeStrategy::Flat};
  std::size_t files = 0;
  std::size_t total = 0;
  std::size_t skipped_rtype = 0;
  std::size_t skipped_parse = 0;
  // Out-of-order count on the merged/dispatched stream — non-zero would
  // indicate a merge bug, not an input defect.
  std::size_t out_of_order_ts_recv = 0;
  // Sum of per-producer within-file out-of-order events — diagnostic on
  // input quality; independent of merge correctness.
  std::size_t producer_out_of_order_ts_recv = 0;
  std::uint64_t first_ts_recv = UNDEF_TIMESTAMP;
  std::uint64_t last_ts_recv = UNDEF_TIMESTAMP;
  double elapsed_sec = 0.0;

  double msgsPerSec() const noexcept {
    return elapsed_sec > 0.0 ? static_cast<double>(total) / elapsed_sec : 0.0;
  }
};

std::vector<std::filesystem::path>
listNdjsonFiles(const std::filesystem::path &folder);

FolderIngestStats ingestFolder(const std::filesystem::path &folder,
                               MergeStrategy strategy,
                               const MarketDataEventConsumer &consumer,
                               FolderIngestOptions options = {});

} // namespace cmf
