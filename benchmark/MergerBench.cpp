// How does each merge strategy scale with the number of data sources (K)?
// Fixed total event volume; sources have zero per-event cost so the measured
// cost is pure merge overhead.

#include "common/BasicTypes.hpp"
#include "merge/HeapMerger.hpp"
#include "merge/LinearScanMerger.hpp"
#include "merge/LoserTreeMerger.hpp"
#include "parser/IMarketDataSource.hpp"
#include "parser/MarketDataEvent.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using namespace cmf;

class VectorSource final : public IMarketDataSource {
public:
  VectorSource(const MarketDataEvent* begin, const MarketDataEvent* end) noexcept
      : begin_(begin), end_(end), cur_(begin) {}

  bool next(MarketDataEvent& out) override {
    if (cur_ == end_) return false;
    out = *cur_++;
    return true;
  }

  void reset() noexcept { cur_ = begin_; }

private:
  const MarketDataEvent* begin_;
  const MarketDataEvent* end_;
  const MarketDataEvent* cur_;
};

std::vector<std::vector<MarketDataEvent>>
buildStreams(std::size_t K, std::size_t total_events) {
  std::vector<std::vector<MarketDataEvent>> streams(K);
  std::mt19937_64                           rng(0xC0FFEE + K);
  std::uniform_int_distribution<NanoTime>   gap(1, 1000);
  std::vector<NanoTime>                     t(K, 0);
  for (std::size_t n = 0; n < total_events; ++n) {
    const std::size_t s = rng() % K;
    t[s] += gap(rng);
    MarketDataEvent ev;
    ev.ts_recv  = t[s];
    ev.ts_event = t[s];
    ev.size     = 10;
    streams[s].push_back(std::move(ev));
  }
  return streams;
}

constexpr std::size_t kTotalEvents = 3'000'000;

const std::vector<std::vector<MarketDataEvent>>& streamsFor(std::size_t K) {
  static std::unordered_map<std::size_t,
                            std::vector<std::vector<MarketDataEvent>>>
      cache;
  auto it = cache.find(K);
  if (it == cache.end())
    it = cache.emplace(K, buildStreams(K, kTotalEvents)).first;
  return it->second;
}

std::vector<VectorSource> makeSources(std::size_t K) {
  const auto& data = streamsFor(K);
  std::vector<VectorSource> sources;
  sources.reserve(data.size());
  for (const auto& stream : data)
    sources.emplace_back(stream.data(), stream.data() + stream.size());
  return sources;
}

template <class Merger>
std::size_t runMerge(benchmark::State& state, Merger& merger) {
  MarketDataEvent out;
  std::size_t     n = 0;
  while (merger.next(out)) {
    benchmark::DoNotOptimize(out.ts_recv);
    ++n;
  }
  if (n != kTotalEvents)
    state.SkipWithError("merged event count mismatch");
  return n;
}

template <template <class> class MergerT>
void BM_MergeByK(benchmark::State& state) {
  const auto  K       = static_cast<std::size_t>(state.range(0));
  auto        sources = makeSources(K);
  std::size_t merged  = 0;
  for (auto _ : state) {
    state.PauseTiming();
    for (auto& s : sources) s.reset();
    std::vector<IMarketDataSource*> ptrs;
    ptrs.reserve(sources.size());
    for (auto& s : sources) ptrs.push_back(&s);
    MergerT<IMarketDataSource> merger(std::move(ptrs));
    state.ResumeTiming();
    merged = runMerge(state, merger);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.iterations() * merged));
  state.counters["K"] = static_cast<double>(K);
}

BENCHMARK_TEMPLATE(BM_MergeByK, LinearScanMerger)
    ->Arg(2)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_MergeByK, HeapMerger)
    ->Arg(2)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_TEMPLATE(BM_MergeByK, LoserTreeMerger)
    ->Arg(2)->Arg(4)->Arg(8)->Arg(16)->Arg(32)->Arg(64)
    ->Unit(benchmark::kMillisecond);

} // namespace
