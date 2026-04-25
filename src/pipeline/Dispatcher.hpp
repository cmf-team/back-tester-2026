// Dispatcher: pulls a globally time-ordered stream from a merger, applies
// each event to the InstrumentBookRegistry, and (optionally) triggers a
// snapshot every N events.
//
// The dispatcher is single-threaded -- by construction the only thread
// that ever mutates the LimitOrderBooks. This is what makes the rest of
// the design simple: snapshotting from a background thread is safe as
// long as it doesn't race with the dispatcher (Snapshotter takes a copy
// while the dispatcher is paused -- see freezeAndSnapshot()).
//
// Routing decisions live in InstrumentBookRegistry, statistics live here.

#pragma once

#include "lob/InstrumentBookRegistry.hpp"
#include "pipeline/IEventSource.hpp"

#include <cstdint>
#include <functional>

namespace cmf {

struct DispatcherStats {
  std::uint64_t events_in{0};       // total events read from the merger
  std::uint64_t events_routed{0};   // applied to a book
  std::uint64_t events_unknown{0};  // could not match order to instrument
  std::uint64_t events_unroutable{0};
  NanoTime      first_ts{0};
  NanoTime      last_ts{0};
};

class Dispatcher {
public:
  // Snapshot callback: invoked every `snapshot_every` events with the
  // current event count and the last applied event's timestamp. Pass 0 to
  // disable.
  using SnapshotHook =
      std::function<void(std::uint64_t event_count, NanoTime last_ts)>;

  Dispatcher(IEventSource &source, InstrumentBookRegistry &registry,
             std::uint64_t snapshot_every = 0,
             SnapshotHook  on_snapshot   = {})
      : source_(source), registry_(registry),
        snapshot_every_(snapshot_every),
        on_snapshot_(std::move(on_snapshot)) {}

  // Drains the source. Returns aggregated statistics.
  DispatcherStats run();

  // Single-step variant for tests / interactive use.
  // Returns false at end of stream.
  bool step();

  const DispatcherStats &stats() const noexcept { return stats_; }

private:
  IEventSource          &source_;
  InstrumentBookRegistry &registry_;
  std::uint64_t          snapshot_every_;
  SnapshotHook           on_snapshot_;

  DispatcherStats        stats_;
};

} // namespace cmf
