#include "transport/HierarchicalSyncedQueue.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace transport {

HierarchicalSyncedQueue::HierarchicalSyncedQueue(
    const std::size_t queues_nums) {
  std::generate_n(std::back_inserter(market_events_queues_), queues_nums,
                  []() { return std::make_shared<MarketEventsQueue>(); });
}

void HierarchicalSyncedQueue::addQueue(
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

bool HierarchicalSyncedQueue::hasNextEvent() {
  const std::lock_guard<std::mutex> lock(m_);
  initializeIfNeeded();
  return !tree_.empty() && tree_[1].valid;
}

MarketDataEvent HierarchicalSyncedQueue::getNextEvent() {
  const std::lock_guard<std::mutex> lock(m_);
  initializeIfNeeded();
  if (tree_.empty() || !tree_[1].valid) {
    return domain::events::EOF_EVENT;
  }

  const Candidate next_event = tree_[1];
  refillSource(next_event.source);
  return next_event.event;
}

std::size_t HierarchicalSyncedQueue::getMarketEventsQueuesSize() const {
  return market_events_queues_.size();
}

const std::vector<MarketEventsQueue::Sptr> &
HierarchicalSyncedQueue::getMarketEventsQueues() const & {
  return market_events_queues_;
}

std::size_t HierarchicalSyncedQueue::nextPowerOfTwo(
    const std::size_t value) {
  std::size_t result = 1;
  while (result < value) {
    result <<= 1U;
  }
  return result;
}

bool HierarchicalSyncedQueue::isEof(const MarketDataEvent &event) noexcept {
  return event.symbol == domain::events::EOF_EVENT.symbol;
}

HierarchicalSyncedQueue::Candidate
HierarchicalSyncedQueue::chooseMin(const Candidate &left,
                                    const Candidate &right) {
  if (!left.valid) {
    return right;
  }
  if (!right.valid) {
    return left;
  }

  if (left.event.hd.ts_event < right.event.hd.ts_event) {
    return left;
  }
  if (right.event.hd.ts_event < left.event.hd.ts_event) {
    return right;
  }
  return (left.source <= right.source) ? left : right;
}

void HierarchicalSyncedQueue::updateSourceFromEvent(
    const std::size_t source, const MarketDataEvent &event) {
  const std::size_t leaf_node = leaf_base_ + source;
  if (isEof(event)) {
    tree_[leaf_node].valid = false;
    tree_[leaf_node].source = source;
    tree_[leaf_node].event = {};
  } else {
    tree_[leaf_node].valid = true;
    tree_[leaf_node].source = source;
    tree_[leaf_node].event = event;
  }

  std::size_t node = leaf_node >> 1U;
  while (node > 0) {
    tree_[node] = chooseMin(tree_[node * 2], tree_[node * 2 + 1]);
    node >>= 1U;
  }
}

void HierarchicalSyncedQueue::refillSource(const std::size_t source) {
  const auto next_event = market_events_queues_.at(source)->popLatestEvent();
  updateSourceFromEvent(source, next_event);
}

void HierarchicalSyncedQueue::initializeIfNeeded() {
  if (initialized_) {
    return;
  }

  if (market_events_queues_.empty()) {
    initialized_ = true;
    return;
  }

  leaf_base_ = nextPowerOfTwo(market_events_queues_.size());
  tree_.assign(leaf_base_ * 2, {});

  for (std::size_t queue_id = 0; queue_id < market_events_queues_.size();
       ++queue_id) {
    const auto next_event = market_events_queues_[queue_id]->popLatestEvent();
    updateSourceFromEvent(queue_id, next_event);
  }

  initialized_ = true;
}

} 
