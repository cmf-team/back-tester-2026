#pragma once
#include "LimitOrderBook.hpp"
#include "MarketDataEvent.hpp"
#include "ThreadSafeQueue.hpp"
#include <atomic>
#include <functional>
#include <thread>
#include <unordered_map>

// ── Dispatcher ─────────────────────────────────────────────────────────────
// Single thread that reads MarketDataEvents from a queue in strict
// chronological order, routes each event to the correct LimitOrderBook,
// and applies it.  All LOB state updates are sequential (no locking needed
// on the LOBs themselves).
//
// Stateless work (snapshots, logging) is dispatched asynchronously via
// detached threads so the dispatcher is never blocked.

class Dispatcher
{
  public:
    using SnapshotCb = std::function<void(uint64_t iid,
                                          const LimitOrderBook&,
                                          uint64_t event_num)>;

    explicit Dispatcher(ThreadSafeQueue<MarketDataEvent>& q,
                        uint64_t snapshot_every = 100'000)
        : queue_(q), snap_every_(snapshot_every) {}

    void set_snapshot_callback(SnapshotCb cb) { snap_cb_ = std::move(cb); }

    // Run synchronously in the caller's thread (call from a std::thread)
    void run()
    {
        while (true)
        {
            auto opt = queue_.pop();
            if (!opt)
                break;
            process(*opt);
        }
    }

    // Accessors (safe after run() returns)
    uint64_t total_events() const { return total_; }
    std::size_t lob_count() const { return lobs_.size(); }

    LimitOrderBook* get_lob(uint64_t iid)
    {
        auto it = lobs_.find(iid);
        return it != lobs_.end() ? &it->second : nullptr;
    }

    void print_summary() const
    {
        std::cout << "\n=== FINAL BEST BID/ASK PER INSTRUMENT ===\n";
        for (auto& [iid, lob] : lobs_)
        {
            auto bid = lob.best_bid();
            auto ask = lob.best_ask();
            std::printf("  instrument %-12llu  bid=%-16s  ask=%s\n",
                        (unsigned long long)iid,
                        bid ? std::to_string((*bid) / 1e9).c_str() : "—",
                        ask ? std::to_string((*ask) / 1e9).c_str() : "—");
        }
    }

  private:
    void process(const MarketDataEvent& ev)
    {
        ++total_;

        // Resolve instrument_id: Cancel/Trade/Fill may arrive with iid=0
        // if the original Add was the only message that carried it.
        uint64_t iid = ev.instrument_id;
        if (iid == 0 && ev.type != EventType::Add)
        {
            auto it = order_iid_.find(ev.order_id);
            if (it != order_iid_.end())
                iid = it->second;
        }
        if (ev.type == EventType::Add && ev.instrument_id != 0)
            order_iid_[ev.order_id] = ev.instrument_id;

        lobs_[iid].apply_event(ev);

        // Periodic async snapshot
        if (snap_every_ && (total_ % snap_every_ == 0) && snap_cb_)
        {
            LimitOrderBook copy = lobs_[iid]; // value copy is safe here
            auto cb = snap_cb_;
            uint64_t t = total_;
            std::thread([cb, iid, copy = std::move(copy), t]() mutable
                        { cb(iid, copy, t); })
                .detach();
        }
    }

    ThreadSafeQueue<MarketDataEvent>& queue_;
    uint64_t snap_every_;
    SnapshotCb snap_cb_;

    std::unordered_map<uint64_t, LimitOrderBook> lobs_;
    std::unordered_map<uint64_t, uint64_t> order_iid_; // order_id→iid cache
    uint64_t total_{0};
};
