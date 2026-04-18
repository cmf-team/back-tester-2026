// EventCollector: stateful sink that records the first N and the last N
// MarketDataEvent instances plus simple aggregates (total / first / last
// timestamp). Used by the HW-1 driver for the "print first 10 / last 10"
// summary.
//
// Until the LOB engine arrives, this is what every event in the file
// ultimately ends up in. A process-wide default instance is exposed via
// defaultEventCollector() so that the free function processMarketDataEvent()
// (which has a fixed signature mandated by the assignment) has somewhere
// stateful to forward to.

#pragma once

#include "common/BasicTypes.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace cmf {

class EventCollector {
public:
  static constexpr std::size_t kDefaultKeep = 10;

  explicit EventCollector(std::size_t keep_first = kDefaultKeep,
                          std::size_t keep_last = kDefaultKeep);

  void operator()(const MarketDataEvent &ev);

  const std::vector<MarketDataEvent> &firstEvents() const noexcept {
    return first_;
  }
  const std::deque<MarketDataEvent> &lastEvents() const noexcept {
    return last_;
  }

  std::uint64_t total() const noexcept { return total_; }
  NanoTime firstTimestamp() const noexcept { return first_ts_; }
  NanoTime lastTimestamp() const noexcept { return last_ts_; }

  void reset();

private:
  std::vector<MarketDataEvent> first_;
  std::deque<MarketDataEvent> last_;
  std::uint64_t total_{0};
  NanoTime first_ts_{0};
  NanoTime last_ts_{0};
  std::size_t keep_first_;
  std::size_t keep_last_;
};

// Process-wide default sink. Used by processMarketDataEvent().
EventCollector &defaultEventCollector();

} // namespace cmf
