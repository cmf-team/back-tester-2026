#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Dispatcher.hpp"
#include "LimitOrderBook.hpp"
#include "MarketDataEvent.hpp"
#include "ThreadSafeQueue.hpp"

// ── ShardedDispatcher ──────────────────────────────────────────────────────
// Shards instruments across N worker threads (each worker owns a subset
// of LOBs). The main dispatcher thread routes events to per-worker queues.
// This gives parallelism across instruments while preserving per-instrument
// chronological order (each instrument always lives on the same worker).
//
// Routing: instrument_id % num_workers → worker index.
// order_id→iid cache lives in the routing thread (single writer, no lock).

class ShardedDispatcher
{
  public:
    explicit ShardedDispatcher(ThreadSafeQueue<MarketDataEvent>& input,
                               std::size_t num_workers = 4,
                               std::size_t queue_size = 100'000)
        : input_(input), num_workers_(num_workers)
    {
        for (std::size_t i = 0; i < num_workers_; ++i)
            worker_queues_.push_back(
                std::make_unique<ThreadSafeQueue<MarketDataEvent>>(queue_size));
    }

    // Run: starts N worker threads + runs the router in the caller's thread.
    // Blocks until all events are processed.
    void run()
    {
        // Start one worker thread per shard
        std::vector<std::thread> workers;
        workers.reserve(num_workers_);
        for (std::size_t i = 0; i < num_workers_; ++i)
        {
            workers.emplace_back([this, i]
                                 { worker_loop(i); });
        }

        // Router: read from input queue, route to correct worker
        while (true)
        {
            auto opt = input_.pop();
            if (!opt)
                break;
            route(*opt);
            ++total_routed_;
        }

        // Signal all workers to stop
        for (auto& q : worker_queues_)
            q->set_done();
        for (auto& t : workers)
            t.join();
    }

    uint64_t total_events() const { return total_routed_; }
    std::size_t worker_count() const { return num_workers_; }

    // Access LOB after run() — thread safe (workers finished)
    LimitOrderBook* get_lob(uint64_t iid)
    {
        std::size_t w = shard(iid);
        auto it = lobs_[w].find(iid);
        return it != lobs_[w].end() ? &it->second : nullptr;
    }

    void print_summary() const
    {
        std::cout << "\n=== SHARDED: FINAL BEST BID/ASK PER INSTRUMENT ===\n";
        for (std::size_t w = 0; w < num_workers_; ++w)
        {
            for (auto& [iid, lob] : lobs_[w])
            {
                auto bid = lob.best_bid();
                auto ask = lob.best_ask();
                std::printf("  [worker %zu] instrument %-12llu  bid=%-16s  ask=%s\n",
                            w, (unsigned long long)iid,
                            bid ? std::to_string((*bid) / 1e9).c_str() : "—",
                            ask ? std::to_string((*ask) / 1e9).c_str() : "—");
            }
        }
    }

    void print_worker_stats() const
    {
        std::cout << "\n  Worker event distribution:\n";
        for (std::size_t i = 0; i < num_workers_; ++i)
            std::printf("    worker %zu: %llu events, %zu instruments\n",
                        i, (unsigned long long)worker_counts_[i], lobs_[i].size());
    }

  private:
    std::size_t shard(uint64_t iid) const
    {
        return iid % num_workers_;
    }

    void route(const MarketDataEvent& ev)
    {
        uint64_t iid = ev.instrument_id;

        // Cancel/Trade/Fill may arrive with iid=0 — resolve via order cache
        if (iid == 0 && ev.type != EventType::Add)
        {
            auto it = order_iid_.find(ev.order_id);
            if (it != order_iid_.end())
                iid = it->second;
        }
        if (ev.type == EventType::Add && ev.instrument_id != 0)
            order_iid_[ev.order_id] = ev.instrument_id;

        // Patch iid into event so worker knows which LOB to update
        MarketDataEvent patched = ev;
        patched.instrument_id = iid;

        worker_queues_[shard(iid)]->push(patched);
    }

    void worker_loop(std::size_t worker_idx)
    {
        auto& queue = *worker_queues_[worker_idx];
        auto& lobs = lobs_[worker_idx];
        uint64_t& count = worker_counts_[worker_idx];

        while (true)
        {
            auto opt = queue.pop();
            if (!opt)
                break;
            lobs[opt->instrument_id].apply_event(*opt);
            ++count;
        }
    }

    ThreadSafeQueue<MarketDataEvent>& input_;
    std::size_t num_workers_;
    std::vector<std::unique_ptr<ThreadSafeQueue<MarketDataEvent>>> worker_queues_;

    // Per-worker state — only touched by that worker (no locking needed)
    std::unordered_map<uint64_t, LimitOrderBook> lobs_[8]; // max 8 workers
    uint64_t worker_counts_[8]{};

    // Routing state — only touched by router thread
    std::unordered_map<uint64_t, uint64_t> order_iid_;
    uint64_t total_routed_{0};
};
