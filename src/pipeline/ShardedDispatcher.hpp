// ShardedDispatcher: parallel multi-instrument dispatcher (HW-2 hard variant
// bonus).
//
// Model:
//
//   merger -> router thread -> N worker queues -> N worker threads
//                                                     \-- own InstrumentBookRegistry
//
//   * The router pulls events one at a time from the (already time-ordered)
//     merger and routes each one into the queue of a worker chosen by
//     hash(instrument_id) % N.
//   * Each worker owns its own InstrumentBookRegistry and applies events
//     sequentially. Per-instrument event order is preserved -- the merger
//     is monotone and a given instrument always lands on the same worker.
//   * Cancel/Modify/Fill without an instrument_id are looked up in a
//     router-side order_id -> worker cache populated on Add.
//
// Snapshots:
//
//   Snapshot requests are propagated as control messages on each worker
//   queue. Every worker captures a snapshot of its own registry when it
//   processes the control message. The resulting snapshots are NOT
//   instantaneously consistent across workers (they are taken at the
//   point each worker drains the request from its queue), which is the
//   trade-off for not stalling the pipeline. For a globally consistent
//   snapshot, use the sequential Dispatcher.
//
// Why message-based snapshotting and not std::barrier:
//   * Keeps every worker fully decoupled (no global synchronization on
//     the hot path).
//   * Per-worker queue depth is the natural skew bound between workers.

#pragma once

#include "lob/InstrumentBookRegistry.hpp"
#include "pipeline/Dispatcher.hpp"
#include "pipeline/EventQueue.hpp"
#include "pipeline/IEventSource.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cmf {

class ShardedDispatcher {
public:
  // Per-worker snapshot hook: same signature as Dispatcher::SnapshotHook,
  // but invoked from the worker thread that owns the registry.
  using SnapshotHook =
      std::function<void(std::size_t worker_idx,
                         const InstrumentBookRegistry &reg,
                         std::uint64_t event_count,
                         NanoTime      last_ts)>;

  ShardedDispatcher(IEventSource &source,
                    std::size_t   num_workers,
                    std::uint64_t snapshot_every = 0,
                    SnapshotHook  on_snapshot   = {},
                    std::size_t   queue_cap     = 1024);

  // Drains the source, joins all workers, returns aggregated stats.
  DispatcherStats run();

  std::size_t numWorkers() const noexcept { return workers_.size(); }
  const InstrumentBookRegistry &registry(std::size_t i) const {
    return workers_[i]->reg;
  }
  const DispatcherStats &workerStats(std::size_t i) const {
    return workers_[i]->stats;
  }

private:
  // Either an event to apply, a snapshot trigger, or a stop sentinel.
  struct Stop {};
  struct SnapshotReq {
    std::uint64_t event_seq;
    NanoTime      last_ts;
  };
  using WorkerMsg = std::variant<MarketDataEvent, SnapshotReq, Stop>;

  struct Worker {
    std::shared_ptr<EventQueue<WorkerMsg>> q;
    InstrumentBookRegistry                  reg;
    std::thread                              thr;
    DispatcherStats                          stats;
    std::size_t                              idx{0};
  };

  void runWorker(Worker &w, const SnapshotHook &hook);
  std::size_t pickWorker(const MarketDataEvent &ev);

  IEventSource &source_;
  std::vector<std::unique_ptr<Worker>> workers_;
  std::uint64_t snapshot_every_;
  SnapshotHook  on_snapshot_;

  // Router-side cache for events whose instrument_id is missing.
  std::unordered_map<OrderId, std::size_t> order_to_worker_;

  // Aggregated stats (filled in by run()).
  DispatcherStats total_stats_;
};

} // namespace cmf
