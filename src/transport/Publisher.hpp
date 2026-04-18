#pragma once

#include "transport/FlatSyncedQueue.hpp"
#include "transport/HierarchicalSyncedQueue.hpp"
#include "transport/Subscriber.hpp"
#include <exception>
#include <memory>
#include <thread>
#include <vector>

namespace transport {
template <typename QueueT> class PublisherT final {
public:
  explicit PublisherT(const std::shared_ptr<QueueT> &market_events_queue_);
  ~PublisherT();

  void addSubscriber(const Consumer &subscriber);
  void run();
  void stop();

private:
  std::vector<Consumer> subscribers_;
  std::shared_ptr<QueueT> market_events_queue_;
  std::unique_ptr<std::thread> process_thread_;
  std::exception_ptr worker_exception_{};
};

using Publisher = PublisherT<FlatSyncedQueue>;
using HierarchicalPublisher = PublisherT<HierarchicalSyncedQueue>;
} // namespace transport
