// EventQueue: a small bounded thread-safe queue based on std::mutex +
// std::condition_variable.
//
// Why this design (and not a lock-free SPSC ring):
//   * The pipeline is producer-bound by JSON parsing, not by queue
//     contention. Profiling on a multi-GB feed shows >95% of time spent
//     parsing; queue overhead is in the noise.
//   * mutex+cv supports both bounded back-pressure (push() blocks when
//     full) and clean shutdown (close() unblocks every waiter). A naive
//     SPSC ring needs an out-of-band "done" flag and busy-spin readers.
//   * The Merger only ever has one consumer per queue, but we want to be
//     able to evolve to a fan-in pattern later; a mutex queue trivially
//     supports MPMC if needed.
//
// The interface intentionally mirrors std::queue closely so unit tests
// can reason about it without spinning up worker threads.

#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace cmf {

template <class T>
class EventQueue {
public:
  // capacity == 0 means "unbounded" (push() never blocks). Useful in tests.
  explicit EventQueue(std::size_t capacity = 1024) noexcept
      : capacity_(capacity) {}

  // Pushes by move. Blocks while the queue is full and not closed.
  // Returns false iff the queue was closed before the value could be enqueued.
  bool push(T value) {
    std::unique_lock lk(m_);
    not_full_.wait(lk, [this] {
      return closed_ || capacity_ == 0 || q_.size() < capacity_;
    });
    if (closed_)
      return false;
    q_.push(std::move(value));
    lk.unlock();
    not_empty_.notify_one();
    return true;
  }

  // Blocks until an element is available or the queue is closed AND drained.
  // Returns std::nullopt only in the latter case (used as a "stream end"
  // sentinel by readers).
  std::optional<T> pop() {
    std::unique_lock lk(m_);
    not_empty_.wait(lk, [this] { return !q_.empty() || closed_; });
    if (q_.empty())
      return std::nullopt; // closed and drained
    T v = std::move(q_.front());
    q_.pop();
    lk.unlock();
    not_full_.notify_one();
    return v;
  }

  // Marks the queue closed; in-flight pushes/pops wake up. Idempotent.
  void close() {
    {
      std::lock_guard lk(m_);
      closed_ = true;
    }
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  bool closed() const noexcept {
    std::lock_guard lk(m_);
    return closed_;
  }

  std::size_t size() const noexcept {
    std::lock_guard lk(m_);
    return q_.size();
  }

  std::size_t capacity() const noexcept { return capacity_; }

private:
  mutable std::mutex      m_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::queue<T>           q_;
  std::size_t             capacity_;
  bool                    closed_{false};
};

} // namespace cmf
