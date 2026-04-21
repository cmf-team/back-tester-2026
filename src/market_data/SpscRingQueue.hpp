// SpscRingQueue — bounded, single-producer / single-consumer lock-free queue.
//
// Contract:
//   * Exactly one producer thread may call push() / close().
//   * Exactly one consumer thread may call pop() / done().
//   * Observers may call empty() / size() / capacity() / isClosed() at any
//     time; results are eventually-consistent snapshots.
//
// Capacity is rounded up to the next power of two (>= 2) so the ring index
// can be computed with a bitwise AND instead of a modulo.

#pragma once

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace cmf {

namespace detail {

constexpr std::size_t nextPow2(std::size_t n) noexcept {
  if (n < 2)
    return 2;
  --n;
  for (std::size_t s = 1; s < sizeof(std::size_t) * 8; s <<= 1)
    n |= n >> s;
  return n + 1;
}

// Typical cache-line size on x86_64 and arm64.
inline constexpr std::size_t kCacheLine = 64;

} // namespace detail

template <typename T> class SpscRingQueue {
  static_assert(std::is_default_constructible_v<T>,
                "SpscRingQueue requires a default-constructible T");
  static_assert(std::is_nothrow_move_assignable_v<T> ||
                    std::is_nothrow_copy_assignable_v<T>,
                "SpscRingQueue requires a nothrow-assignable T for safety");

public:
  explicit SpscRingQueue(std::size_t requested_capacity)
      : cap_(detail::nextPow2(requested_capacity)), mask_(cap_ - 1),
        slots_(cap_) {
    if (requested_capacity == 0)
      throw std::invalid_argument("SpscRingQueue: capacity must be >= 1");
  }

  SpscRingQueue(const SpscRingQueue &) = delete;
  SpscRingQueue &operator=(const SpscRingQueue &) = delete;
  SpscRingQueue(SpscRingQueue &&) = delete;
  SpscRingQueue &operator=(SpscRingQueue &&) = delete;

  // Producer-side. Returns false when full.
  bool push(T value) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    if (tail - head == cap_)
      return false;
    slots_[tail & mask_] = std::move(value);
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  // Consumer-side. Returns false when empty.
  bool pop(T &out) {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail)
      return false;
    out = std::move(slots_[head & mask_]);
    head_.store(head + 1, std::memory_order_release);
    return true;
  }

  // Producer-side. Called when the producer has no more items.
  void close() noexcept { closed_.store(true, std::memory_order_release); }

  bool isClosed() const noexcept {
    return closed_.load(std::memory_order_acquire);
  }

  // True iff the producer has closed AND the consumer has drained it.
  bool done() const noexcept { return isClosed() && empty(); }

  bool empty() const noexcept {
    return head_.load(std::memory_order_acquire) ==
           tail_.load(std::memory_order_acquire);
  }

  // Snapshot size. May lag real value by a few items in the contended case.
  std::size_t size() const noexcept {
    const std::size_t t = tail_.load(std::memory_order_acquire);
    const std::size_t h = head_.load(std::memory_order_acquire);
    return t - h;
  }

  std::size_t capacity() const noexcept { return cap_; }

private:
  const std::size_t cap_;
  const std::size_t mask_;
  std::vector<T> slots_;

  // Separate hot atomics on distinct cache lines to avoid false sharing
  // between producer (tail_) and consumer (head_).
  alignas(detail::kCacheLine) std::atomic<std::size_t> head_{0};
  alignas(detail::kCacheLine) std::atomic<std::size_t> tail_{0};
  alignas(detail::kCacheLine) std::atomic<bool> closed_{false};
};

} // namespace cmf
