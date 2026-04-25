// ShardedDispatcher — bonus parallelism: events for one instrument always
// land on the same worker, preserving per-instrument chronological order
// without locking any LOB.
//
// Routing strategy: hash(instrument_id) % num_workers.
// The router thread (= caller thread of dispatch()) maintains the
// order_id -> instrument_id cache (single-writer, no lock). After
// resolution it pushes a copy of the event into one of N SPSC queues.
// Each worker thread owns its own slice of LOBs and an OrderState cache
// for the orders that resolved into its shard, so Cancel/Modify/Trade
// references stay local to one worker.

#pragma once

#include "common/BasicTypes.hpp"
#include "market_data/MarketDataEvent.hpp"
#include "market_data/SpscRingQueue.hpp"
#include "order_book/LimitOrderBook.hpp"
#include "order_book/OrderState.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cmf
{

struct ShardedDispatcherStats
{
    std::uint64_t events_total{0};
    std::uint64_t events_routed{0};
    std::uint64_t unresolved_iid{0};
    std::vector<std::uint64_t> per_worker_events;
    std::vector<std::uint64_t> per_worker_instruments;
    std::vector<std::uint64_t> per_worker_orders_active;
};

class ShardedDispatcher
{
  public:
    static constexpr std::size_t kMaxWorkers = 8;

    // num_workers must be in [1, kMaxWorkers]. queue_capacity is rounded up
    // to next power of two by SpscRingQueue.
    explicit ShardedDispatcher(std::size_t num_workers,
                               std::size_t queue_capacity = 16 * 1024);

    ShardedDispatcher(const ShardedDispatcher&) = delete;
    ShardedDispatcher& operator=(const ShardedDispatcher&) = delete;

    ~ShardedDispatcher();

    // Called from the strict-chronological dispatcher thread of
    // HardTask::runHardTask. Resolves iid, picks a shard, pushes onto its
    // SPSC queue (blocks via yield on backpressure).
    void dispatch(const MarketDataEvent& event);

    // Closes worker queues and joins all worker threads. Returns aggregate
    // stats; idempotent.
    ShardedDispatcherStats finalize();

  private:
    struct Worker
    {
        std::unique_ptr<SpscRingQueue<MarketDataEvent>> queue;
        std::unordered_map<std::uint64_t, LimitOrderBook> books;
        std::unordered_map<OrderId, OrderState> orders;
        std::uint64_t events{0};
        std::thread thread;
    };

    std::size_t shardOf(std::uint64_t iid) const noexcept;
    static void runWorker(Worker& w);

    std::size_t num_workers_;
    std::vector<std::unique_ptr<Worker>> workers_;
    std::unordered_map<OrderId, std::uint64_t> order_to_iid_;
    ShardedDispatcherStats stats_{};
    bool finalized_{false};
};

} // namespace cmf
