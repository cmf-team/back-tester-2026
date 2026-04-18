#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include <benchmark/benchmark.h>

#include <thread>

namespace {

using BlockingQueue = cmf::BlockingQueue<cmf::MarketDataEvent>;
using LockFreeQueue = cmf::LockFreeQueue<cmf::MarketDataEvent, (1 << 13)>;

} // namespace

// TODO: Add more complex scenarios with different producer/consumer speeds,
// e.g., slow consumer with fast producer, fast consumer with slow producer
template <typename Queue>
static void BM_Queue_SPSC_Concurrent(benchmark::State &state) {
  const int64_t N = state.range(0);

  for (auto _ : state) {
    state.PauseTiming();
    Queue q;
    state.ResumeTiming();

    std::atomic<int64_t> processed{0};

    std::thread producer([&] {
      for (int64_t i = 0; i < N; ++i)
        q.push(cmf::MarketDataEvent{i, i * 2});
      q.close();
    });

    std::thread consumer([&] {
      while (q.pop([&](cmf::MarketDataEvent &&) { ++processed; })) {
      }
    });

    producer.join();
    consumer.join();

    benchmark::DoNotOptimize(processed.load());
  }
  state.SetItemsProcessed(state.iterations() * N);
}

#define REGISTER_QUEUE_BENCH(BenchFn, QueueType, Label)                        \
  BENCHMARK_TEMPLATE(BenchFn, QueueType)                                       \
      ->Name(Label)                                                            \
      ->Arg(1 << 12)                                                           \
      ->Arg(1 << 13)                                                           \
      ->Arg(1 << 14)                                                           \
      ->Arg(1 << 15)                                                           \
      ->Unit(benchmark::kMicrosecond)                                          \
      ->Iterations(5)                                                          \
      ->Repetitions(3)                                                         \
      ->DisplayAggregatesOnly()

REGISTER_QUEUE_BENCH(BM_Queue_SPSC_Concurrent, BlockingQueue,
                     "BlockingQueue/SPSC");
REGISTER_QUEUE_BENCH(BM_Queue_SPSC_Concurrent, LockFreeQueue,
                     "LockFreeQueue/SPSC");