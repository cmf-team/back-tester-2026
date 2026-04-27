#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include <benchmark/benchmark.h>

#include <barrier>
#include <memory>
#include <thread>

namespace
{

using BlockingQueue = cmf::BlockingQueue<cmf::MarketDataEvent>;
using LockFreeQueue = cmf::LockFreeQueue<cmf::MarketDataEvent, (1 << 14)>;
using CompactBlockingQueue = cmf::BlockingQueue<cmf::CompactMarketDataEvent>;
using CompactLockFreeQueue = cmf::LockFreeQueue<cmf::CompactMarketDataEvent, (1 << 14)>;

template <typename T>
T make_event(int64_t i);

template <>
cmf::MarketDataEvent make_event<cmf::MarketDataEvent>(int64_t i)
{
    return {i, i * 2};
}

template <>
cmf::CompactMarketDataEvent make_event<cmf::CompactMarketDataEvent>(int64_t i)
{
    return {i, static_cast<uint64_t>(i * 2)};
}

} // namespace

template <typename Queue>
static void BM_Queue_SPSC_Concurrent(benchmark::State& state)
{
    using Event = typename Queue::value_type;

    const int64_t N = state.range(0);

    std::unique_ptr<Queue> q_ptr;
    std::atomic<bool> stop{false};
    std::atomic<int64_t> processed{0};

    // 3-party barriers: benchmark thread + producer + consumer
    std::barrier sync_start{3};
    std::barrier sync_done{3};

    std::thread producer([&]
                         {
        while (true) {
            sync_start.arrive_and_wait();
            if (stop.load(std::memory_order_acquire)) {
                sync_done.arrive_and_wait();
                break;
            }
            for (int64_t i = 0; i < N; ++i)
                q_ptr->push(make_event<Event>(i));
            q_ptr->close();
            sync_done.arrive_and_wait();
        } });

    std::thread consumer([&]
                         {
        while (true) {
            sync_start.arrive_and_wait();
            if (stop.load(std::memory_order_acquire)) {
                sync_done.arrive_and_wait();
                break;
            }
            while (q_ptr->pop([&](Event&&) { ++processed; })) {}
            sync_done.arrive_and_wait();
        } });

    for (auto _ : state)
    {
        state.PauseTiming();
        processed.store(0, std::memory_order_relaxed);
        q_ptr = std::make_unique<Queue>();
        state.ResumeTiming();

        sync_start.arrive_and_wait(); // release workers; timed region begins
        sync_done.arrive_and_wait();  // wait for both workers to finish

        benchmark::DoNotOptimize(processed.load());
    }
    state.SetItemsProcessed(state.iterations() * N);

    // Clean shutdown: signal stop, give workers a valid queue, then release
    stop.store(true, std::memory_order_release);
    q_ptr = std::make_unique<Queue>();
    sync_start.arrive_and_wait();
    sync_done.arrive_and_wait();
    producer.join();
    consumer.join();
}

#define REGISTER_QUEUE_BENCH(BenchFn, QueueType, Label) \
    BENCHMARK_TEMPLATE(BenchFn, QueueType)              \
        ->Name(Label)                                   \
        ->Arg(1 << 12)                                  \
        ->Arg(1 << 13)                                  \
        ->Unit(benchmark::kMicrosecond)                 \
        ->MinWarmUpTime(0.5)                            \
        ->MinTime(1.0)                                  \
        ->Repetitions(10)                               \
        ->DisplayAggregatesOnly()                       \
        ->UseRealTime()

REGISTER_QUEUE_BENCH(BM_Queue_SPSC_Concurrent, BlockingQueue,
                     "BlockingQueue/SPSC");
REGISTER_QUEUE_BENCH(BM_Queue_SPSC_Concurrent, LockFreeQueue,
                     "LockFreeQueue/SPSC");
REGISTER_QUEUE_BENCH(BM_Queue_SPSC_Concurrent, CompactBlockingQueue,
                     "CompactBlockingQueue/SPSC");
REGISTER_QUEUE_BENCH(BM_Queue_SPSC_Concurrent, CompactLockFreeQueue,
                     "CompactLockFreeQueue/SPSC");