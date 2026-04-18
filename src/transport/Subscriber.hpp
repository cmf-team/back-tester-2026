#pragma once

#include "common/Events.hpp"
#include "transport/FlatSyncedQueue.hpp"
#include "transport/HierarchicalSyncedQueue.hpp"
#include <exception>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace transport {
using MarketDataEvent = domain::events::MarketDataEvent;
using EventHandler = std::function<void(const MarketDataEvent &)>;
using EndEventsHandler = std::function<void()>;

struct Consumer final {
  std::string name;
  EventHandler onEvent;
  EndEventsHandler onEndEvents;
};

template <typename QueueT> class QueueSubscriberT final {
public:
  explicit QueueSubscriberT(const std::shared_ptr<QueueT> &market_events_queue);
  ~QueueSubscriberT();

  QueueSubscriberT(const QueueSubscriberT &) = delete;
  QueueSubscriberT &operator=(const QueueSubscriberT &) = delete;
  QueueSubscriberT(QueueSubscriberT &&) = delete;
  QueueSubscriberT &operator=(QueueSubscriberT &&) = delete;

  void addSubscriber(Consumer subscriber);
  void run();
  void stop();

private:
  std::vector<Consumer> consumers_;
  std::shared_ptr<QueueT> market_events_queue_;
  std::thread worker_{};
  std::exception_ptr worker_exception_{};
};

using QueueSubscriber = QueueSubscriberT<FlatSyncedQueue>;
using HierarchicalQueueSubscriber = QueueSubscriberT<HierarchicalSyncedQueue>;
} // namespace transport
