#include "pipeline/Snapshotter.hpp"

#include "parser/MarketDataEvent.hpp"

#include <iomanip>
#include <ostream>
#include <utility>

namespace cmf {

Snapshotter::Snapshotter(InstrumentBookRegistry &reg, std::ostream &out,
                         std::size_t depth, std::size_t queue_cap)
    : reg_(reg), out_(out), depth_(depth),
      q_(std::make_shared<EventQueue<GlobalSnapshot>>(queue_cap)) {}

Snapshotter::~Snapshotter() { stop(); }

void Snapshotter::start() {
  if (worker_.joinable())
    return;
  worker_ = std::thread([this] { runWriter(); });
}

void Snapshotter::stop() {
  if (q_)
    q_->close();
  if (worker_.joinable())
    worker_.join();
}

GlobalSnapshot Snapshotter::capture(std::uint64_t event_seq,
                                    NanoTime      last_ts) const {
  GlobalSnapshot s;
  s.event_seq   = event_seq;
  s.last_ts     = last_ts;
  s.instruments = reg_.size();
  s.per_instrument.reserve(reg_.size());
  reg_.forEach([&](InstrumentId iid, const LimitOrderBook &book) {
    InstrumentSnapshot is;
    is.iid         = iid;
    is.bid_levels  = book.bidLevels();
    is.ask_levels  = book.askLevels();
    is.open_orders = book.openOrders();
    is.top_bids    = book.topBids(depth_);
    is.top_asks    = book.topAsks(depth_);
    s.per_instrument.push_back(std::move(is));
  });
  return s;
}

void Snapshotter::submit(GlobalSnapshot snap) {
  if (q_)
    q_->push(std::move(snap));
}

// captureAndSubmit -- the single entry-point the dispatcher uses as its
// SnapshotHook. The dispatcher invokes this callback every
// `snapshot_every` events, after applying the N-th event but before
// pulling the next one. Splitting the work into capture() + submit()
// is intentional:
//
//   1. capture() runs SYNCHRONOUSLY on the dispatcher thread. Because
//      the dispatcher is the *only* writer to the books (single-writer
//      invariant), this guarantees the snapshot is internally
//      consistent: no event can be applied while we are copying the
//      registry, and we don't need any LOB-level locking.
//
//   2. submit() hands the captured GlobalSnapshot to the writer thread
//      via a bounded queue. Formatting + actual I/O happen there, off
//      the hot path. If the writer falls behind, the bounded queue
//      back-pressures the dispatcher (which is preferable to
//      unbounded memory growth).
//
// Parameters:
//   * event_seq -- 1-based index of the event we just applied (used as
//     the "@seq=" tag in the snapshot header).
//   * last_ts   -- ts_event of that event; this is the EXCHANGE time
//     of the snapshot, not wall-clock.
//
// Result: the snapshot is enqueued; emission of the formatted block is
// asynchronous and observable via `emitted()` once written.
void Snapshotter::captureAndSubmit(std::uint64_t event_seq, NanoTime last_ts) {
  submit(capture(event_seq, last_ts));
}

void Snapshotter::runWriter() {
  while (true) {
    auto snap = q_->pop();
    if (!snap)
      break;

    std::lock_guard lk(out_mu_);
    out_ << "[snapshot @seq=" << snap->event_seq
         << " ts=" << formatIso8601Nanos(snap->last_ts)
         << " instruments=" << snap->instruments << "]\n";
    for (const auto &is : snap->per_instrument) {
      out_ << "  iid=" << is.iid << " bids=" << is.bid_levels
           << " asks=" << is.ask_levels << " orders=" << is.open_orders;
      if (!is.top_bids.empty()) {
        out_ << " top_bid=" << std::fixed << std::setprecision(5)
             << is.top_bids.front().first << '@' << is.top_bids.front().second;
      }
      if (!is.top_asks.empty()) {
        out_ << " top_ask=" << std::fixed << std::setprecision(5)
             << is.top_asks.front().first << '@' << is.top_asks.front().second;
      }
      out_ << '\n';
    }
    ++emitted_;
  }
}

} // namespace cmf
