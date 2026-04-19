#pragma once

#include <atomic>
#include <memory>
#include <optional>

namespace cmf {

// Simple lock-free SPSC (Single Producer Single Consumer) queue
// Based on the classic circular buffer with atomic head/tail pointers
template <typename T, size_t Capacity = 8192> class LockFreeQueue {
private:
  struct alignas(64) Node {
    std::atomic<bool> ready{false};
    T data;
  };

  alignas(64) std::atomic<size_t> head_{0};
  alignas(64) std::atomic<size_t> tail_{0};
  std::unique_ptr<Node[]> buffer_;
  static constexpr size_t capacity_ = Capacity;

public:
  LockFreeQueue() : buffer_(std::make_unique<Node[]>(capacity_)) {}

  // Producer: Try to push an item
  bool tryPush(const T &item) {
    size_t current_tail = tail_.load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % capacity_;

    // Check if queue is full
    if (next_tail == head_.load(std::memory_order_acquire)) {
      return false;
    }

    buffer_[current_tail].data = item;
    buffer_[current_tail].ready.store(true, std::memory_order_release);
    tail_.store(next_tail, std::memory_order_release);

    return true;
  }

  // Producer: Push with move semantics
  bool tryPush(T &&item) {
    size_t current_tail = tail_.load(std::memory_order_relaxed);
    size_t next_tail = (current_tail + 1) % capacity_;

    // Check if queue is full
    if (next_tail == head_.load(std::memory_order_acquire)) {
      return false;
    }

    buffer_[current_tail].data = std::move(item);
    buffer_[current_tail].ready.store(true, std::memory_order_release);
    tail_.store(next_tail, std::memory_order_release);

    return true;
  }

  // Consumer: Try to pop an item
  std::optional<T> tryPop() {
    size_t current_head = head_.load(std::memory_order_relaxed);

    // Check if queue is empty
    if (current_head == tail_.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    // Wait for data to be ready
    if (!buffer_[current_head].ready.load(std::memory_order_acquire)) {
      return std::nullopt;
    }

    T item = std::move(buffer_[current_head].data);
    buffer_[current_head].ready.store(false, std::memory_order_release);

    size_t next_head = (current_head + 1) % capacity_;
    head_.store(next_head, std::memory_order_release);

    return item;
  }

  // Check if queue is empty (approximate - may be stale)
  bool empty() const {
    return head_.load(std::memory_order_relaxed) ==
           tail_.load(std::memory_order_relaxed);
  }

  // Get approximate size (may be stale)
  size_t size() const {
    size_t h = head_.load(std::memory_order_relaxed);
    size_t t = tail_.load(std::memory_order_relaxed);
    if (t >= h) {
      return t - h;
    }
    return capacity_ - h + t;
  }

  size_t capacity() const { return capacity_; }
};

} // namespace cmf
