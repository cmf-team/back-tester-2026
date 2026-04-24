#include "transport/FlatSyncedQueue.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace transport {

bool FlatSyncedQueue::ItemCompare::operator()(const Item &lhs,
                                              const Item &rhs) const {
  if (lhs.event.hd.ts_event != rhs.event.hd.ts_event) {
    return lhs.event.hd.ts_event > rhs.event.hd.ts_event;
  }
  return lhs.id > rhs.id;
}

FlatSyncedQueue::FlatSyncedQueue(const std::size_t queues_nums) {
  std::generate_n(std::back_inserter(market_events_queues_), queues_nums,
                  []() { return std::make_shared<MarketEventsQueue>(); });
}

void FlatSyncedQueue::addQueue(
    const MarketEventsQueue::Sptr &market_events_queue) {
  const std::lock_guard<std::mutex> lock(m_);
  if (initialized_) {
    throw std::logic_error("Cannot add queue after merge initialization");
  }
  if (!market_events_queue) {
    throw std::invalid_argument("Queue must not be null");
  }
  market_events_queues_.push_back(market_events_queue);
}

bool FlatSyncedQueue::hasNextEvent() {
  const std::lock_guard<std::mutex> lock(m_);
  initializeIfNeeded();
  return !merged_events_.empty();
}

MarketDataEvent FlatSyncedQueue::getNextEvent() {
  const std::lock_guard<std::mutex> lock(m_);

  initializeIfNeeded();
  if (merged_events_.empty()) {
    return domain::events::EOF_EVENT;
  }

  const auto next_event = merged_events_.top();
  merged_events_.pop();

  const auto next_to_merge =
      market_events_queues_.at(next_event.id)->popLatestEvent();
  if (next_to_merge.symbol != domain::events::EOF_EVENT.symbol) {
    merged_events_.push({next_event.id, next_to_merge});
  }

  return next_event.event;
}

std::size_t FlatSyncedQueue::getMarketEventsQueuesSize() const {
  return market_events_queues_.size();
}

const std::vector<MarketEventsQueue::Sptr> &
FlatSyncedQueue::getMarketEventsQueues() const & {
  return market_events_queues_;
}

void FlatSyncedQueue::initializeIfNeeded() {
  if (initialized_) {
    return;
  }

  for (std::size_t queue_id = 0; queue_id < market_events_queues_.size();
       ++queue_id) {
    const auto next_event = market_events_queues_[queue_id]->popLatestEvent();
    if (next_event.symbol != domain::events::EOF_EVENT.symbol) {
      merged_events_.push({queue_id, next_event});
    }
  }
  initialized_ = true;
}

} 
