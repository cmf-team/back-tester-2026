#pragma once

#include "transport/MarketEventQueue.hpp"
#include <mutex>
#include <queue>
#include <vector>

namespace transport {
class FlatSyncedQueue final {
public:
  using Sptr = std::shared_ptr<FlatSyncedQueue>;

private:
  struct Item {
    std::size_t id;
    MarketDataEvent event;
  };

  struct ItemCompare {
    bool operator()(const Item &lhs, const Item &rhs) const;
  };

public:
  explicit FlatSyncedQueue(std::size_t queues_nums);

  void addQueue(const MarketEventsQueue::Sptr &market_events_queue);

  bool hasNextEvent();
  MarketDataEvent getNextEvent();

  std::size_t getMarketEventsQueuesSize() const;

  const std::vector<MarketEventsQueue::Sptr> &
  getMarketEventsQueues() const &;

private:
  void initializeIfNeeded();

  std::mutex m_;
  bool initialized_{false};

  std::priority_queue<Item, std::vector<Item>, ItemCompare> merged_events_;
  std::vector<MarketEventsQueue::Sptr> market_events_queues_;
};
} // namespace transport
