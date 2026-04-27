// Running statistics over a stream of MarketDataEvents.
// Extend by adding new private fields + updating onEvent + operator<<.

#pragma once

#include "parser/MarketDataEvent.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <string>
#include <unordered_map>

namespace cmf {

class Statistics {
public:
  // Aggregates tracked for every unique instrument_id seen in the stream.
  struct InstrumentStats {
    uint64_t    count{0};
    NanoTime    first_ts{std::numeric_limits<NanoTime>::max()};
    NanoTime    last_ts{std::numeric_limits<NanoTime>::min()};
    int64_t     min_price{std::numeric_limits<int64_t>::max()};
    int64_t     max_price{std::numeric_limits<int64_t>::min()};
    // Count of events whose ts_recv was strictly less than the previous
    // ts_recv for this instrument. Docs guarantee monotonic ts_recv per
    // symbol, so any increment here signals a data-quality issue.
    uint64_t    ts_recv_regressions{0};
    double      avg_delta_ns{0.0};
    std::string symbol;          // last-seen symbol (serves as symbol dictionary)
  };

  void onEvent(const MarketDataEvent& ev) noexcept;
  void reset() noexcept;

  // --- global ---
  uint64_t totalEvents() const noexcept { return total_events_; }
  NanoTime firstTs() const noexcept { return first_ts_; }
  NanoTime lastTs()  const noexcept { return last_ts_; }

  // --- per action (indexed by the char value of Action) ---
  uint64_t actionCount(Action a) const noexcept {
    return action_counts_[static_cast<uint8_t>(a)];
  }

  // --- per publisher / per instrument ---
  const std::unordered_map<uint16_t, uint64_t>& publisherCounts() const noexcept {
    return publisher_counts_;
  }
  const std::unordered_map<uint32_t, InstrumentStats>& instrumentStats() const noexcept {
    return instrument_stats_;
  }

  // --- global price range (across events with a defined price) ---
  int64_t minPrice() const noexcept { return min_price_; }
  int64_t maxPrice() const noexcept { return max_price_; }
  bool    hasPrice() const noexcept { return min_price_ != std::numeric_limits<int64_t>::max(); }

  // --- gap / data-quality indicators ---
  uint64_t maybeBadBookEvents() const noexcept { return maybe_bad_book_events_; }
  uint64_t badTsRecvEvents()    const noexcept { return bad_ts_recv_events_; }
  uint64_t tsRecvRegressions()  const noexcept { return ts_recv_regressions_; }
  double tsAvgDeltaNs()         const noexcept { return avg_delta_ns_; }

private:
  uint64_t total_events_{0};
  NanoTime first_ts_{std::numeric_limits<NanoTime>::max()};
  NanoTime last_ts_{std::numeric_limits<NanoTime>::min()};

  std::array<uint64_t, 128> action_counts_{};   // indexed by uint8_t(action_char)

  std::unordered_map<uint16_t, uint64_t>        publisher_counts_;
  std::unordered_map<uint32_t, InstrumentStats> instrument_stats_;

  int64_t  min_price_{std::numeric_limits<int64_t>::max()};
  int64_t  max_price_{std::numeric_limits<int64_t>::min()};

  uint64_t maybe_bad_book_events_{0};
  uint64_t bad_ts_recv_events_{0};
  uint64_t ts_recv_regressions_{0};
  double   avg_delta_ns_{0.0};
};

// Human-readable summary (ISO8601 timestamps, decimal prices, sorted breakdowns).
std::ostream& operator<<(std::ostream& os, const Statistics& s);

} // namespace cmf
