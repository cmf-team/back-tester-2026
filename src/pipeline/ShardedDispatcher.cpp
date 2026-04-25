#include "pipeline/ShardedDispatcher.hpp"

#include <algorithm>
#include <utility>

namespace cmf {

ShardedDispatcher::ShardedDispatcher(IEventSource &source,
                                     std::size_t   num_workers,
                                     std::uint64_t snapshot_every,
                                     SnapshotHook  on_snapshot,
                                     std::size_t   queue_cap)
    : source_(source), snapshot_every_(snapshot_every),
      on_snapshot_(std::move(on_snapshot)) {
  if (num_workers == 0)
    num_workers = 1;
  workers_.reserve(num_workers);
  for (std::size_t i = 0; i < num_workers; ++i) {
    auto w = std::make_unique<Worker>();
    w->idx = i;
    w->q = std::make_shared<EventQueue<WorkerMsg>>(queue_cap);
    workers_.push_back(std::move(w));
  }
}

std::size_t ShardedDispatcher::pickWorker(const MarketDataEvent &ev) {
  // Try to use the router's order_id -> worker cache when the event lacks
  // instrument_id (typical for Cancel / Modify / Fill on some feeds).
  if (ev.instrument_id != 0)
    return ev.instrument_id % workers_.size();

  if (ev.action == Action::Cancel || ev.action == Action::Modify ||
      ev.action == Action::Fill) {
    auto it = order_to_worker_.find(ev.order_id);
    if (it != order_to_worker_.end())
      return it->second;
  }

  // Last resort: fall back to worker 0 (router-level "unroutable").
  return 0;
}

DispatcherStats ShardedDispatcher::run() {
  // 1. Spin up workers.
  for (auto &w : workers_) {
    w->thr = std::thread([this, wp = w.get()] { runWorker(*wp, on_snapshot_); });
  }

  // 2. Router loop.
  MarketDataEvent ev;
  std::uint64_t event_seq = 0;
  while (source_.next(ev)) {
    if (event_seq == 0)
      total_stats_.first_ts = ev.timestamp;
    total_stats_.last_ts = ev.timestamp;
    ++event_seq;
    ++total_stats_.events_in;

    const std::size_t w_idx = pickWorker(ev);

    // Maintain the order_id -> worker cache.
    if (ev.action == Action::Add) {
      order_to_worker_[ev.order_id] = w_idx;
    } else if (ev.action == Action::Cancel || ev.action == Action::Fill) {
      // Eviction is best-effort; the worker may keep the order alive on
      // partial cancels/fills, so we only erase here when the size value
      // strongly implies a full cancel (size==0). This is harmless: the
      // worst case is a stale cache entry that may resolve to the wrong
      // worker if the order_id is reused, which doesn't happen in practice.
      if (ev.size == 0)
        order_to_worker_.erase(ev.order_id);
    }

    workers_[w_idx]->q->push(WorkerMsg{ev});

    if (snapshot_every_ != 0 && (event_seq % snapshot_every_) == 0) {
      // Broadcast the snapshot request to every worker.
      for (auto &w : workers_)
        w->q->push(WorkerMsg{SnapshotReq{event_seq, ev.timestamp}});
    }
  }

  // 3. Stop and join workers; aggregate stats.
  for (auto &w : workers_)
    w->q->push(WorkerMsg{Stop{}});
  for (auto &w : workers_)
    if (w->thr.joinable())
      w->thr.join();

  for (auto &w : workers_) {
    total_stats_.events_routed     += w->stats.events_routed;
    total_stats_.events_unknown    += w->stats.events_unknown;
    total_stats_.events_unroutable += w->stats.events_unroutable;
  }

  return total_stats_;
}

void ShardedDispatcher::runWorker(Worker &w, const SnapshotHook &hook) {
  while (true) {
    auto msg = w.q->pop();
    if (!msg)
      break; // queue closed
    bool stop = false;
    std::visit(
        [&](auto &&payload) {
          using T = std::decay_t<decltype(payload)>;
          if constexpr (std::is_same_v<T, MarketDataEvent>) {
            switch (w.reg.apply(payload)) {
            case InstrumentBookRegistry::RouteResult::Applied:
              ++w.stats.events_routed;
              break;
            case InstrumentBookRegistry::RouteResult::UnknownOrder:
              ++w.stats.events_unknown;
              break;
            case InstrumentBookRegistry::RouteResult::Unroutable:
              ++w.stats.events_unroutable;
              break;
            }
            if (w.stats.events_in == 0)
              w.stats.first_ts = payload.timestamp;
            w.stats.last_ts = payload.timestamp;
            ++w.stats.events_in;
          } else if constexpr (std::is_same_v<T, SnapshotReq>) {
            if (hook)
              hook(w.idx, w.reg, payload.event_seq, payload.last_ts);
          } else if constexpr (std::is_same_v<T, Stop>) {
            stop = true;
          }
        },
        *msg);
    if (stop)
      break;
  }
}

} // namespace cmf
