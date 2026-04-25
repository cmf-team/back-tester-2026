// Snapshotter: emits periodic snapshots of every InstrumentBookRegistry
// asynchronously, without blocking the dispatcher hot loop.
//
// Design:
//
//   * The dispatcher thread (which is the *only* writer to the books) calls
//     captureAndSubmit() at fixed event-count intervals. capture() runs
//     inline on the dispatcher thread, so it observes a stable book by
//     construction (no locking required).
//
//   * The captured snapshot is enqueued into a bounded queue and a worker
//     thread does the heavy I/O (formatting + writing). This keeps the
//     dispatcher free to keep applying events.
//
//   * A snapshot is a deep copy: top-N price/qty per side per instrument
//     plus minimal metadata. Memory cost is O(instruments * depth), which
//     is negligible compared to the book itself.
//
// We deliberately do not buffer all snapshots in memory -- pulled from
// the queue, formatted to the chosen ostream, dropped.

#pragma once

#include "lob/InstrumentBookRegistry.hpp"
#include "lob/LimitOrderBook.hpp"
#include "pipeline/EventQueue.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace cmf {

struct InstrumentSnapshot {
  InstrumentId iid{};
  std::size_t  bid_levels{0};
  std::size_t  ask_levels{0};
  std::size_t  open_orders{0};
  std::vector<std::pair<double, LimitOrderBook::Qty>> top_bids;
  std::vector<std::pair<double, LimitOrderBook::Qty>> top_asks;
};

struct GlobalSnapshot {
  std::uint64_t event_seq{0};
  NanoTime      last_ts{0};
  std::size_t   instruments{0};
  std::vector<InstrumentSnapshot> per_instrument;
};

class Snapshotter {
public:
  Snapshotter(InstrumentBookRegistry &reg, std::ostream &out,
              std::size_t depth = 5, std::size_t queue_cap = 32);
  ~Snapshotter();

  Snapshotter(const Snapshotter &) = delete;
  Snapshotter &operator=(const Snapshotter &) = delete;

  void start();
  void stop();   // closes queue, joins worker

  // Synchronously copy the current state of the registry; safe to call
  // from the dispatcher thread.
  GlobalSnapshot capture(std::uint64_t event_seq, NanoTime last_ts) const;

  // Hand the snapshot off to the writer thread (non-blocking unless the
  // writer can't keep up, in which case we apply back-pressure).
  void submit(GlobalSnapshot snap);

  // Convenience: capture+submit. Use as a Dispatcher::SnapshotHook.
  void captureAndSubmit(std::uint64_t event_seq, NanoTime last_ts);

  std::uint64_t emitted() const noexcept { return emitted_.load(); }

private:
  void runWriter();

  InstrumentBookRegistry &reg_;
  std::ostream           &out_;
  std::size_t             depth_;

  std::shared_ptr<EventQueue<GlobalSnapshot>> q_;
  std::thread             worker_;
  std::atomic<std::uint64_t> emitted_{0};
  // Guards multi-threaded access to `out_` (matter only if external code
  // also writes there).
  std::mutex              out_mu_;
};

} // namespace cmf
