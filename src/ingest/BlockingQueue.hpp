// Simple bounded blocking queue with close() support.
//
// Used by the hard-variant ingest pipeline to connect producer threads,
// merger threads, and the single dispatcher thread without unbounded growth.

#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <utility>

namespace cmf {

template <class T> class BlockingQueue {
public:
  explicit BlockingQueue(std::size_t capacity)
      : capacity_{capacity == 0 ? 1 : capacity} {}

  BlockingQueue(const BlockingQueue &) = delete;
  BlockingQueue &operator=(const BlockingQueue &) = delete;

  bool push(T value) {
    std::unique_lock lock{mutex_};
    not_full_.wait(lock, [&] { return closed_ || queue_.size() < capacity_; });
    if (closed_) {
      return false;
    }
    queue_.push_back(std::move(value));
    lock.unlock();
    not_empty_.notify_one();
    return true;
  }

  bool pop(T &out) {
    std::unique_lock lock{mutex_};
    not_empty_.wait(lock, [&] { return closed_ || !queue_.empty(); });
    if (queue_.empty()) {
      return false;
    }
    out = std::move(queue_.front());
    queue_.pop_front();
    lock.unlock();
    not_full_.notify_one();
    return true;
  }

  void close() {
    std::lock_guard lock{mutex_};
    closed_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
  }

private:
  std::size_t capacity_;
  std::deque<T> queue_;
  bool closed_ = false;
  std::mutex mutex_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
};

} // namespace cmf
