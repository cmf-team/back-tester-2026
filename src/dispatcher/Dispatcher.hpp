// Dispatcher — single-threaded fan-out from a strict-chronological event
// stream into per-instrument LimitOrderBooks.
//
// Responsibilities:
//   * resolve order_id -> instrument_id for messages that arrive without it
//     (Cancel/Trade/Fill always reference an Add observed earlier),
//   * translate MarketDataEvent::action into LOB level deltas,
//   * snapshot a few BBO entries every N events to a SnapshotWorker.
//
// Sequential: this object lives entirely on the dispatcher thread of
// HardTask::runHardTask, so there is no internal synchronisation.

#pragma once

#include "common/BasicTypes.hpp"
#include "dispatcher/SnapshotWorker.hpp"
#include "market_data/MarketDataEvent.hpp"
#include "order_book/LimitOrderBook.hpp"
#include "order_book/OrderState.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <unordered_map>

namespace cmf
{

struct DispatcherStats
{
    std::uint64_t events_total{0};
    std::uint64_t events_routed{0};
    std::uint64_t orders_active{0};
    std::uint64_t instruments_touched{0};
    std::uint64_t unresolved_iid{0};
};

class Dispatcher
{
  public:
    struct Options
    {
        std::size_t snapshot_every{0}; // 0 disables snapshots
        std::size_t snapshot_max_instruments{8};
        std::ostream* snapshot_out{nullptr}; // nullptr disables I/O thread
    };

    Dispatcher();
    explicit Dispatcher(Options opts);

    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    // Routes one event into the right LimitOrderBook. Maintains the order
    // cache. Triggers a snapshot every `snapshot_every` events.
    void dispatch(const MarketDataEvent& event);

    // Stops the snapshot worker (joins its thread) and returns final stats.
    // Safe to call multiple times; idempotent.
    DispatcherStats finalize();

    // Read-only views.
    const std::unordered_map<std::uint64_t, LimitOrderBook>& books() const noexcept
    {
        return books_;
    }
    const DispatcherStats& stats() const noexcept { return stats_; }

  private:
    void maybeEmitSnapshot();

    Options opts_;
    std::unordered_map<std::uint64_t, LimitOrderBook> books_;
    std::unordered_map<OrderId, OrderState> orders_;
    SnapshotWorker snapshot_worker_;
    DispatcherStats stats_{};
    bool finalized_{false};
};

// Pretty-prints final BBO for every touched instrument, sorted by id.
void printDispatcherSummary(std::ostream& os, const Dispatcher& d);

} // namespace cmf
