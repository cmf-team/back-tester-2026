#include "market_data/HardTask.hpp"

#include "market_data/Consumer.hpp"
#include "market_data/EventSource.hpp"
#include "market_data/FileProducer.hpp"
#include "market_data/MarketDataEvent.hpp"
#include "market_data/Merger.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace cmf {

std::string toString(MergerKind kind) {
  switch (kind) {
  case MergerKind::Flat:
    return "Flat";
  case MergerKind::Hierarchy:
    return "Hierarchy";
  }
  return "Unknown";
}

std::vector<std::filesystem::path>
listMboJsonFiles(const std::filesystem::path &dir) {
  namespace fs = std::filesystem;
  std::vector<fs::path> files;
  for (const auto &entry : fs::directory_iterator(dir)) {
    if (!entry.is_regular_file())
      continue;
    const auto name = entry.path().filename().string();
    if (name.size() < 10)
      continue;
    // Match Databento naming: *.mbo.json
    if (name.find(".mbo.json") != std::string::npos)
      files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

namespace {

// FNV-1a 64-bit step. Used to order-sensitively fingerprint the dispatched
// stream so we can cross-check Flat vs Hierarchy produced identical output.
constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ULL;
constexpr std::uint64_t kFnvPrime = 0x100000001b3ULL;

inline std::uint64_t fnv1aMix(std::uint64_t h, std::uint64_t x) noexcept {
  // Mix 8 bytes little-endian.
  for (int i = 0; i < 8; ++i) {
    h ^= static_cast<std::uint8_t>(x & 0xFF);
    h *= kFnvPrime;
    x >>= 8;
  }
  return h;
}

std::unique_ptr<IMerger>
makeMerger(MergerKind kind,
           const std::vector<IEventSource *> &sources) {
  switch (kind) {
  case MergerKind::Flat:
    return std::make_unique<FlatMerger>(sources);
  case MergerKind::Hierarchy:
    return std::make_unique<HierarchyMerger>(sources);
  }
  throw std::invalid_argument("HardTask: unknown MergerKind");
}

} // namespace

BenchmarkResult runHardTask(const std::vector<std::filesystem::path> &files,
                            MergerKind kind, bool verbose,
                            std::size_t queue_capacity, EventSink sink) {
  BenchmarkResult result;
  result.strategy = toString(kind);

  if (files.empty())
    return result;

  // 1) Build producers (creates queues, no threads yet).
  std::vector<std::unique_ptr<FileProducer>> producers;
  producers.reserve(files.size());
  for (const auto &f : files)
    producers.push_back(std::make_unique<FileProducer>(f, queue_capacity));

  // 2) Start producer threads BEFORE constructing the merger — the merger's
  //    constructor blocks on each source's first event.
  const auto t_start = std::chrono::steady_clock::now();
  for (auto &p : producers)
    p->start();

  // 3) Wrap each queue as an IEventSource.
  std::vector<std::unique_ptr<SpscQueueSource>> sources_owned;
  sources_owned.reserve(producers.size());
  std::vector<IEventSource *> sources;
  sources.reserve(producers.size());
  for (auto &p : producers) {
    sources_owned.push_back(std::make_unique<SpscQueueSource>(p->queue()));
    sources.push_back(sources_owned.back().get());
  }

  // 4) Build merger. This may block briefly while it seeds from each source.
  auto merger = makeMerger(kind, sources);

  // 5) Dispatcher thread — drains the merged stream in a dedicated OS thread
  //    as required by the task spec ("Launches one dispatcher thread that
  //    reads sequentially from the merged queue"). Results are captured into
  //    outer-scope locals by reference.
  std::uint64_t total = 0;
  NanoTime first_ts = 0;
  NanoTime last_ts = 0;
  std::uint64_t fingerprint = kFnvOffset;
  std::exception_ptr dispatcher_error;

  std::thread dispatcher([&] {
    try {
      MarketDataEvent event;
      NanoTime prev_ts = std::numeric_limits<NanoTime>::min();
      while (merger->next(event)) {
        if (event.ts_recv < prev_ts) {
          throw std::runtime_error(
              "HardTask: chronological order violated at event #" +
              std::to_string(total));
        }
        prev_ts = event.ts_recv;
        if (total == 0)
          first_ts = event.ts_recv;
        last_ts = event.ts_recv;

        fingerprint =
            fnv1aMix(fingerprint, static_cast<std::uint64_t>(event.ts_recv));
        fingerprint = fnv1aMix(fingerprint, event.instrument_id);
        fingerprint = fnv1aMix(fingerprint, event.order_id);
        fingerprint =
            fnv1aMix(fingerprint, static_cast<std::uint64_t>(event.sequence));

        if (sink)
          sink(event);
        if (verbose)
          processMarketDataEvent(event);

        ++total;
      }
    } catch (...) {
      dispatcher_error = std::current_exception();
      // Drain remaining events to unblock producers (they would otherwise
      // spin forever on queue->push backpressure with no consumer).
      try {
        MarketDataEvent discard;
        while (merger->next(discard)) {
        }
      } catch (...) {
        // Ignore secondary errors during error-path drain.
      }
    }
  });

  dispatcher.join();

  // 6) Join producers; propagate any error (dispatcher's first).
  for (auto &p : producers)
    p->join();

  const auto t_end = std::chrono::steady_clock::now();

  if (dispatcher_error)
    std::rethrow_exception(dispatcher_error);

  for (auto &p : producers) {
    if (auto eptr = p->error()) {
      std::rethrow_exception(eptr);
    }
  }

  result.total = total;
  result.first_ts = first_ts;
  result.last_ts = last_ts;
  result.elapsed =
      std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start);
  result.fingerprint = fingerprint;
  return result;
}

void printBenchmarkResult(std::ostream &os, const BenchmarkResult &r) {
  const double secs =
      static_cast<double>(r.elapsed.count()) / 1e9;
  const double mps = r.throughputMps();

  std::ostringstream line;
  line << std::left << std::setw(10) << (r.strategy + ":")
       << " total=" << std::setw(12) << r.total
       << " elapsed=" << std::fixed << std::setprecision(3) << secs << " s"
       << " throughput=" << std::fixed << std::setprecision(0) << mps
       << " msg/s"
       << " first_ts=" << r.first_ts << " last_ts=" << r.last_ts
       << " fp=0x" << std::hex << r.fingerprint << std::dec;
  os << line.str() << '\n';
}

} // namespace cmf
