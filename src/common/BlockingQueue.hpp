#pragma once

#include "QueueBase.hpp"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <utility>

namespace cmf {
template <typename T>
class BlockingQueue : public QueueBase<BlockingQueue<T>, T> {
  friend class QueueBase<BlockingQueue<T>, T>;

public:
  BlockingQueue() = default;
  BlockingQueue(const BlockingQueue &) = delete;
  BlockingQueue &operator=(const BlockingQueue &) = delete;
  BlockingQueue(BlockingQueue &&) = delete;
  BlockingQueue &operator=(BlockingQueue &&) = delete;
  ~BlockingQueue();

private:
  void push_impl(T value);

  void push_batch_impl(T *items, std::size_t count);

  template <typename Fn>
    requires std::invocable<Fn, T &&>
  [[nodiscard]] bool pop_impl(Fn &&fn);

  void close_impl() noexcept;

  [[nodiscard]] bool is_closed_impl() const noexcept;
  [[nodiscard]] bool empty_impl() const noexcept;
  [[nodiscard]] std::size_t size_impl() const noexcept;

  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
  std::queue<T> queue_;
  bool closed_ = false;
};

template <typename T> BlockingQueue<T>::~BlockingQueue() { close_impl(); }

template <typename T> void BlockingQueue<T>::push_impl(T value) {
  {
    std::scoped_lock lock{mutex_};
    if (closed_) [[unlikely]]
      throw std::runtime_error("BlockingQueue::push: queue is closed");
    queue_.push(std::move(value));
  }
  not_empty_.notify_one();
}

template <typename T>
void BlockingQueue<T>::push_batch_impl(T *items, std::size_t count) {
  {
    std::scoped_lock lock{mutex_};
    if (closed_) [[unlikely]]
      throw std::runtime_error("BlockingQueue::push_batch: queue is closed");
    for (std::size_t i = 0; i < count; ++i)
      queue_.push(std::move(items[i]));
  }
  not_empty_.notify_all();
}

template <typename T>
template <typename Fn>
  requires std::invocable<Fn, T &&>
bool BlockingQueue<T>::pop_impl(Fn &&fn) {
  std::unique_lock lock{mutex_};
  not_empty_.wait(lock, [this] { return !queue_.empty() || closed_; });

  if (queue_.empty()) [[unlikely]]
    return false;

  T item = std::move(queue_.front());
  queue_.pop();
  lock.unlock();

  std::forward<Fn>(fn)(std::move(item));
  return true;
}

template <typename T> void BlockingQueue<T>::close_impl() noexcept {
  {
    std::scoped_lock lock{mutex_};
    closed_ = true;
  }
  not_empty_.notify_all();
}

template <typename T> bool BlockingQueue<T>::is_closed_impl() const noexcept {
  std::scoped_lock lock{mutex_};
  return closed_;
}

template <typename T> bool BlockingQueue<T>::empty_impl() const noexcept {
  std::scoped_lock lock{mutex_};
  return queue_.empty();
}

template <typename T> std::size_t BlockingQueue<T>::size_impl() const noexcept {
  std::scoped_lock lock{mutex_};
  return queue_.size();
}
} // namespace cmf
