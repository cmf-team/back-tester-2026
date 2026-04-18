#include "common/BlockingQueue.hpp"
#include "common/MarketDataEvent.hpp"
#include "ingestion/FlatMerger.hpp"
#include "ingestion/HierarchyMerger.hpp"
#include "ingestion/IngestionPipeline.hpp"
#include "ingestion/NativeDataParser.hpp"
#include <benchmark/benchmark.h>

#include <deque>
#include <filesystem>
#include <thread>
#include <vector>

template <typename T> using LFQ = cmf::LockFreeQueue<T, (1 << 13)>;

static void BM_App_FullPipeline(benchmark::State &state) {
  const char *env = std::getenv("BENCH_DIR");
  if (!env) {
    state.SkipWithError("BENCH_DIR env var not set.");
    return;
  }
  const std::filesystem::path dir(env);
  if (!std::filesystem::is_directory(dir)) {
    state.SkipWithError("BENCH_DIR is not a directory.");
    return;
  }

  static constexpr std::string_view kSuffix = ".mbo.json";
  std::vector<std::filesystem::path> files;
  for (const auto &entry : std::filesystem::directory_iterator(dir))
    if (entry.is_regular_file() && entry.path().string().ends_with(kSuffix))
      files.push_back(entry.path());

  if (files.empty()) {
    state.SkipWithError("No .mbo.json files found in BENCH_DIR directory.");
    return;
  }

  const std::size_t N = files.size();
  int64_t total_events = 0;

  for (auto _ : state) {
    state.PauseTiming();
    std::deque<cmf::BlockingQueue<cmf::MarketDataEvent>> file_queues(N);
    cmf::BlockingQueue<cmf::MarketDataEvent> merged_queue;
    state.ResumeTiming();

    cmf::FlatMerger<cmf::BlockingQueue, cmf::BlockingQueue> merger(
        file_queues, merged_queue);
    std::thread merger_thread([&] { merger.run_impl(); });

    std::vector<std::thread> producers;
    producers.reserve(N);
    for (std::size_t i = 0; i < N; ++i) {
      producers.emplace_back([&file_queues, &files, i]() {
        auto push_fn = [&file_queues, i](const cmf::MarketDataEvent &e) {
          file_queues[i].push(e);
        };
        cmf::IngestionPipeline<cmf::NativeDataParser, decltype(push_fn)>
            pipeline(files[i], push_fn);
        pipeline.ingest();
        file_queues[i].close();
      });
    }

    int64_t count = 0;
    while (merged_queue.pop([&](cmf::MarketDataEvent &&) { ++count; })) {
    }

    for (auto &t : producers)
      t.join();
    merger_thread.join();

    benchmark::DoNotOptimize(count);
    total_events = count;
  }
  state.SetItemsProcessed(state.iterations() * total_events);
}

BENCHMARK(BM_App_FullPipeline)
    ->Unit(benchmark::kMillisecond)
    ->Iterations(3)
    ->Repetitions(3)
    ->DisplayAggregatesOnly();
