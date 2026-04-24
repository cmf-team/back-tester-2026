#include "transport/Publisher.hpp"

#include <exception>
#include <stdexcept>

namespace transport {

template <typename QueueT>
PublisherT<QueueT>::PublisherT(
    const std::shared_ptr<QueueT> &market_events_queue_)
    : market_events_queue_(market_events_queue_) {}

template <typename QueueT> PublisherT<QueueT>::~PublisherT() { stop(); }

template <typename QueueT>
void PublisherT<QueueT>::addSubscriber(const Consumer &subscriber) {
  subscribers_.push_back(subscriber);
}

template <typename QueueT> void PublisherT<QueueT>::run() {
  if (process_thread_ && process_thread_->joinable()) {
    throw std::logic_error("PublisherT::run: already running");
  }
  worker_exception_ = nullptr;
  process_thread_ = std::make_unique<std::thread>([this]() {
    try {
      auto event = market_events_queue_->getNextEvent();
      while (event.symbol != domain::events::EOF_EVENT.symbol) {
        for (const auto &subscriber : subscribers_) {
          subscriber.onEvent(event);
        }
        event = market_events_queue_->getNextEvent();
      }
    } catch (...) {
      worker_exception_ = std::current_exception();
    }
  });
}

template <typename QueueT> void PublisherT<QueueT>::stop() {
  if (!process_thread_ || !process_thread_->joinable()) {
    return;
  }
  process_thread_->join();
  process_thread_.reset();
  if (worker_exception_) {
    std::rethrow_exception(worker_exception_);
  }
  for (const auto &subscriber : subscribers_) {
    subscriber.onEndEvents();
  }
}

template class PublisherT<FlatSyncedQueue>;
template class PublisherT<HierarchicalSyncedQueue>;

} 
