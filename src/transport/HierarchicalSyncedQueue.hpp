#pragma once

#include "transport/MarketEventQueue.hpp"
#include <mutex>
#include <vector>

namespace transport {
class HierarchicalSyncedQueue final {
public:
  using Sptr = std::shared_ptr<HierarchicalSyncedQueue>;

private:
  struct Candidate {
    std::size_t source = 0;
    MarketDataEvent event{};
    bool valid = false;
  };

public:
  explicit HierarchicalSyncedQueue(std::size_t queues_nums);

  void addQueue(const MarketEventsQueue::Sptr &market_events_queue);

  bool hasNextEvent();
  MarketDataEvent getNextEvent();

  std::size_t getMarketEventsQueuesSize() const;

  const std::vector<MarketEventsQueue::Sptr> &
  getMarketEventsQueues() const &;

private:
  static std::size_t nextPowerOfTwo(const std::size_t value);
  static bool isEof(const MarketDataEvent &event) noexcept;
  static Candidate chooseMin(const Candidate &left, const Candidate &right);

  void updateSourceFromEvent(const std::size_t source,
                                const MarketDataEvent &event);
  void refillSource(const std::size_t source);
  void initializeIfNeeded();

  std::mutex m_;
  bool initialized_{false};
  std::size_t leaf_base_{1};

  std::vector<Candidate> tree_;
  std::vector<MarketEventsQueue::Sptr> market_events_queues_;
};
} // namespace transport
