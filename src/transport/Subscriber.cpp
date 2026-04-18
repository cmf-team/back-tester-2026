#include "transport/Subscriber.hpp"

#include <exception>
#include <stdexcept>
#include <thread>

namespace transport {

template <typename QueueT>
QueueSubscriberT<QueueT>::QueueSubscriberT(
    const std::shared_ptr<QueueT> &market_events_queue)
    : market_events_queue_(market_events_queue) {}

template <typename QueueT> QueueSubscriberT<QueueT>::~QueueSubscriberT() {
  stop();
}

template <typename QueueT> void QueueSubscriberT<QueueT>::stop() {
  if (!worker_.joinable()) {
    return;
  }
  worker_.join();
  if (worker_exception_) {
    std::rethrow_exception(worker_exception_);
  }
  for (const auto &subscriber : consumers_) {
    subscriber.onEndEvents();
  }
}

template <typename QueueT>
void QueueSubscriberT<QueueT>::addSubscriber(Consumer subscriber) {
  consumers_.push_back(std::move(subscriber));
}

template <typename QueueT> void QueueSubscriberT<QueueT>::run() {
  if (worker_.joinable()) {
    throw std::logic_error("QueueSubscriberT::run: already running");
  }
  worker_exception_ = nullptr;
  worker_ = std::thread([this]() {
    try {
      auto event = market_events_queue_->getNextEvent();

      while (event.symbol != domain::events::EOF_EVENT.symbol) {
        for (const auto &subscriber : consumers_) {
          subscriber.onEvent(event);
        }
        event = market_events_queue_->getNextEvent();
      }
    } catch (...) {
      worker_exception_ = std::current_exception();
    }
  });
}

template class QueueSubscriberT<FlatSyncedQueue>;
template class QueueSubscriberT<HierarchicalSyncedQueue>;

} // namespace transport
