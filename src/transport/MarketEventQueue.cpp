#include "transport/MarketEventQueue.hpp"

namespace transport {

void MarketEventsQueue::put(const MarketDataEvent &event) {
  const std::lock_guard<std::mutex> lock(m_);
  events_.push_back(event);
  cv_.notify_one();
}

MarketDataEvent MarketEventsQueue::popLatestEvent() {
  std::unique_lock<std::mutex> lock(m_);
  cv_.wait(lock, [this] { return !events_.empty(); });
  const MarketDataEvent event = events_.front();
  events_.pop_front();
  return event;
}

std::size_t MarketEventsQueue::size() const { return events_.size(); }

} // namespace transport
