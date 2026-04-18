#pragma once

#include "common/Events.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

namespace transport {
using MarketDataEvent = domain::events::MarketDataEvent;

class MarketEventsQueue final {
public:
  using Sptr = std::shared_ptr<MarketEventsQueue>;

  void put(const MarketDataEvent &event);
  MarketDataEvent popLatestEvent();
  std::size_t size() const;

private:
  std::mutex m_;
  std::condition_variable cv_;
  std::deque<MarketDataEvent> events_;
};
} // namespace transport
