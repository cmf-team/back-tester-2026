// Consumer — downstream sink for MarketDataEvents.
//
// The free function `processMarketDataEvent` matches the signature specified
// in task_1.md and is the eventual hand-off point to the LOB engine. For now
// it just prints the event for verification.

#pragma once

#include "common/BasicTypes.hpp"
#include "market_data/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>

namespace cmf {

// Prints the event details to stdout (via MarketDataEvent::operator<<).
// Task-mandated signature — do not change.
void processMarketDataEvent(const MarketDataEvent &event);

// Aggregate stats collected during ingestion of one or more NDJSON files.
struct IngestionSummary {
  std::uint64_t total{0};
  NanoTime first_ts{0};
  NanoTime last_ts{0};
};

// Runs the Standard-task pipeline over a single NDJSON file.
// - Parses every line into a MarketDataEvent.
// - Remembers the first `head_n` and last `tail_n` events.
// - Writes a readable summary to `out` (total, first/last ts, head and tail
//   samples, each rendered via processMarketDataEvent-equivalent output).
//
// Returns accumulated statistics.
// Throws std::runtime_error on IO/parse failure.
IngestionSummary runStandardTask(const std::filesystem::path &path,
                                 std::size_t head_n, std::size_t tail_n,
                                 std::ostream &out);

} // namespace cmf
