#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include "ingestion/FlatMerger.hpp"
#include "ingestion/HierarchyMerger.hpp"
#include <benchmark/benchmark.h>

#include <deque>
#include <thread>

template <typename Merger>
static void BM_Merger_Merge(benchmark::State& state)
{
    const int N = static_cast<int>(state.range(0));
    const int EV = 10'000;

    int64_t total = 0;
    for (auto _ : state)
    {
        state.PauseTiming();
        std::deque<cmf::BlockingQueue<cmf::MarketDataEvent>> queues(N);
        for (int q = 0; q < N; ++q)
        {
            for (int i = 0; i < EV; ++i)
            {
                cmf::MarketDataEvent e{};
                e.ts_recv = static_cast<int64_t>(q + i * N);
                queues[q].push(e);
            }
            queues[q].close();
        }
        cmf::BlockingQueue<cmf::MarketDataEvent> output;
        state.ResumeTiming();

        int64_t count = 0;
        std::thread t([&]
                      {
      Merger m(queues, output);
      m.run(); });
        while (output.pop([&](cmf::MarketDataEvent&&)
                          { ++count; }))
        {
        }
        t.join();
        benchmark::DoNotOptimize(count);
        total = count;
    }
    state.SetItemsProcessed(state.iterations() * total);
}

#define REGISTER_MERGER_BENCH(Type)           \
    BENCHMARK_TEMPLATE(BM_Merger_Merge, Type) \
        ->Arg(2)                              \
        ->Arg(4)                              \
        ->Arg(8)                              \
        ->Arg(16)                             \
        ->Unit(benchmark::kMillisecond)       \
        ->Iterations(3)                       \
        ->Repetitions(3)                      \
        ->DisplayAggregatesOnly()

REGISTER_MERGER_BENCH(cmf::FlatMerger<>);
REGISTER_MERGER_BENCH(cmf::HierarchyMerger<>);
