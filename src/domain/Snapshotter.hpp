#pragma once

#include "common/Events.hpp"
#include "domain/LimitOrderBook.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <vector>

namespace domain {


struct InstrumentSnapshot {
  InstrumentId instrument_id;
  BidsBookMap bids;
  AsksBookMap asks;
  BestQuote best_bid;
  BestQuote best_ask;
};


struct LobSnapshot {
  std::size_t events_seen{};
  std::uint64_t at_ts_event_ns{};
  std::vector<InstrumentSnapshot> instruments;
};

std::ostream &operator<<(std::ostream &os, const LobSnapshot &snap);

template <typename QueueT> class Snapshotter final {
public:
  using NanoDuration = std::uint64_t;

  Snapshotter(LimitOrderBook<QueueT> &limit_order_book,
              NanoDuration interval_ns);

  void onEvent(const MarketDataEvent &event);
  void onEndEvents();

  const std::vector<LobSnapshot> &snapshots() const;
  NanoDuration intervalNs() const;
  std::size_t eventsSeen() const;

  std::uint64_t firstEventTsNs() const;

  void printFinalBestQuotes(std::ostream &os) const;

private:
  void captureSnapshot(std::uint64_t at_ts_event_ns);

  LimitOrderBook<QueueT> &limit_order_book_;
  NanoDuration interval_ns_;
  std::optional<std::uint64_t> first_event_ts_ns_;
  std::optional<std::uint64_t> last_snapshot_ts_ns_;
  std::size_t events_seen_{0};
  std::vector<LobSnapshot> snapshots_;
};

}
