#include "domain/Snapshotter.hpp"

#include "transport/FlatSyncedQueue.hpp"
#include "transport/HierarchicalSyncedQueue.hpp"

#include <algorithm>
#include <ostream>
#include <set>
#include <unordered_set>
#include <utility>

namespace domain {
namespace {

void printBest(std::ostream &os, const BestQuote &q) {
  if (!q.has_value()) {
    os << "none";
  } else {
    os << q->first << "x" << q->second;
  }
}

}

std::ostream &operator<<(std::ostream &os, const LobSnapshot &snap) {
  os << "  snapshot ts=" << snap.at_ts_event_ns
     << " events=" << snap.events_seen << '\n';
  for (const auto &i : snap.instruments) {
    os << "    " << i.instrument_id << " bid=";
    printBest(os, i.best_bid);
    os << " ask=";
    printBest(os, i.best_ask);
    os << '\n';
  }
  return os;
}

template <typename QueueT>
Snapshotter<QueueT>::Snapshotter(LimitOrderBook<QueueT> &limit_order_book,
                                 const NanoDuration interval_ns)
    : limit_order_book_(limit_order_book), interval_ns_(interval_ns) {}

template <typename QueueT>
void Snapshotter<QueueT>::onEvent(const MarketDataEvent &event) {
  ++events_seen_;
  const auto ts = event.hd.ts_event;

  if (!last_snapshot_ts_ns_.has_value()) {


    first_event_ts_ns_ = ts;
    last_snapshot_ts_ns_ = ts;
    return;
  }

  if (interval_ns_ == 0) {
    return;
  }

  if (ts - *last_snapshot_ts_ns_ >= interval_ns_) {
    captureSnapshot(ts);
    last_snapshot_ts_ns_ = ts;
  }
}

template <typename QueueT> void Snapshotter<QueueT>::onEndEvents() {}

template <typename QueueT>
const std::vector<LobSnapshot> &Snapshotter<QueueT>::snapshots() const {
  return snapshots_;
}

template <typename QueueT>
typename Snapshotter<QueueT>::NanoDuration
Snapshotter<QueueT>::intervalNs() const {
  return interval_ns_;
}

template <typename QueueT> std::size_t Snapshotter<QueueT>::eventsSeen() const {
  return events_seen_;
}

template <typename QueueT>
std::uint64_t Snapshotter<QueueT>::firstEventTsNs() const {
  return first_event_ts_ns_.value_or(0ULL);
}

template <typename QueueT>
void Snapshotter<QueueT>::printFinalBestQuotes(std::ostream &os) const {
  // Safe to query the LOB directly: by the time main() calls this, the
  // pipeline has been stopped, all worker threads joined, no freeze() needed.
  const auto best_bids = limit_order_book_.getAllBestBids();
  const auto best_asks = limit_order_book_.getAllBestAsks();
  std::set<InstrumentId> ids;
  for (const auto &[id, _] : best_bids) {
    ids.insert(id);
  }
  for (const auto &[id, _] : best_asks) {
    ids.insert(id);
  }
  os << "  final:\n";
  for (const auto id : ids) {
    os << "    " << id << " bid=";
    const auto bit = best_bids.find(id);
    printBest(os, bit == best_bids.end() ? BestQuote{} : bit->second);
    os << " ask=";
    const auto ait = best_asks.find(id);
    printBest(os, ait == best_asks.end() ? BestQuote{} : ait->second);
    os << '\n';
  }
}

template <typename QueueT>
void Snapshotter<QueueT>::captureSnapshot(const std::uint64_t at_ts_event_ns) {




  auto view = limit_order_book_.freeze();

  LobSnapshot snap;
  snap.events_seen = events_seen_;
  snap.at_ts_event_ns = at_ts_event_ns;

  const auto bids = limit_order_book_.getAllBids();
  const auto asks = limit_order_book_.getAllAsks();
  const auto best_bids = limit_order_book_.getAllBestBids();
  const auto best_asks = limit_order_book_.getAllBestAsks();

  std::unordered_set<InstrumentId> ids;
  ids.reserve(bids.size() + asks.size());
  for (const auto &[id, _] : bids) {
    ids.insert(id);
  }
  for (const auto &[id, _] : asks) {
    ids.insert(id);
  }

  snap.instruments.reserve(ids.size());
  for (const auto id : ids) {
    InstrumentSnapshot instr;
    instr.instrument_id = id;
    if (const auto it = bids.find(id); it != bids.end()) {
      instr.bids = it->second;
    }
    if (const auto it = asks.find(id); it != asks.end()) {
      instr.asks = it->second;
    }
    if (const auto it = best_bids.find(id); it != best_bids.end()) {
      instr.best_bid = it->second;
    }
    if (const auto it = best_asks.find(id); it != best_asks.end()) {
      instr.best_ask = it->second;
    }
    snap.instruments.push_back(std::move(instr));
  }
  std::sort(snap.instruments.begin(), snap.instruments.end(),
            [](const InstrumentSnapshot &a, const InstrumentSnapshot &b) {
              return a.instrument_id < b.instrument_id;
            });

  snapshots_.push_back(std::move(snap));
}

template class Snapshotter<transport::FlatSyncedQueue>;
template class Snapshotter<transport::HierarchicalSyncedQueue>;

}
