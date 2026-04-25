// HardTask — multi-file ingestion with two k-way merge strategies.

#pragma once

#include "common/BasicTypes.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace cmf {

struct MarketDataEvent;

// Stateless visitor invoked by the dispatcher thread for every event in
// strict chronological order. Used by Task-2 to fan out into per-instrument
// LimitOrderBooks. An empty function (default) preserves Task-1 behaviour.
using EventSink = std::function<void(const MarketDataEvent &)>;

enum class MergerKind : std::uint8_t { Flat, Hierarchy };

std::string toString(MergerKind kind);

struct BenchmarkResult {
  std::string strategy;
  std::uint64_t total{0};
  NanoTime first_ts{0};
  NanoTime last_ts{0};
  std::chrono::nanoseconds elapsed{0};
  // Order-sensitive 64-bit fingerprint over the dispatched stream. Two
  // mergers over the same input must produce identical fingerprints.
  std::uint64_t fingerprint{0};

  double throughputMps() const noexcept {
    if (elapsed.count() <= 0)
      return 0.0;
    return static_cast<double>(total) * 1e9 /
           static_cast<double>(elapsed.count());
  }
};

// Lists *.mbo.json files in `dir` (non-recursive) sorted lexicographically.
std::vector<std::filesystem::path>
listMboJsonFiles(const std::filesystem::path &dir);

// Runs the full Hard-task pipeline:
//   * spawns one FileProducer per file,
//   * builds the requested merger,
//   * acts as the single dispatcher thread that drains the merger and calls
//     processMarketDataEvent (if verbose) while verifying chronological order.
//
// Throws std::runtime_error on any producer error or chronology violation.
BenchmarkResult runHardTask(const std::vector<std::filesystem::path> &files,
                            MergerKind kind, bool verbose,
                            std::size_t queue_capacity = 64 * 1024,
                            EventSink sink = {});

// Pretty-prints a BenchmarkResult to `os`.
void printBenchmarkResult(std::ostream &os, const BenchmarkResult &r);

} // namespace cmf
