// Lock-free single-producer / single-consumer bounded queue with hysteresis
// backpressure. Same public API and semantics as AsyncQueue.hpp, but the fast
// path has no mutex — push/pop touch only the producer's and consumer's own
// indices, plus a single acquire load of the other side's index.
//
// Design:
//   * Capacity is rounded up to a power of 2 so we can mask instead of mod.
//   * `head_` (consumer-owned) and `tail_` (producer-owned) are monotonically
//     increasing 64-bit counters. Both use release on store / acquire on
//     cross-thread load. Indices into the ring are `idx & mask_`.
//   * `tail_ - head_` is the live size. We reserve all `cap_` slots — the
//     indices are independent so there's no head==tail ambiguity.
//   * Two dedicated wake-counters back the parking paths. Each blocked side
//     does load-token / re-check / wait(token); the other side fetch-adds
//     and notifies. Counter-based wakeups are lost-wakeup-free: any state
//     change observably bumps the counter, so a wait posted after the bump
//     sees a value mismatch and returns immediately.
//       producer_wakeup_ — bumped by consumer on the low-water crossing
//                          and by requestStop().
//       consumer_wakeup_ — bumped by producer on empty->non-empty pushes,
//                          and by close() / requestStop().
//   * Hysteresis: producer parks at cap, exits only at <= cap/2 — consumer
//     emits exactly one wake per cap/2 fill cycle.
//   * Atomics are aligned to 64 bytes to avoid false sharing.
//
// Caveats:
//   * Strictly single-producer / single-consumer. Concurrent producers (or
//     consumers) on the same queue corrupt the indices.
//   * `T` should be cheap to move; the ring stores T by value.

#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace cmf {

template <class T>
class SpscAsyncQueue {
 public:
  // Cache-line size. 64 is correct for x86-64 and Apple silicon (the L1
  // cache is 128B but the destructive interference unit is still 64B for
  // these atomic patterns). Hardcoded — `std::hardware_destructive_-
  // interference_size` is unreliably exposed across toolchains.
  static constexpr std::size_t kCacheLine = 64;

  explicit SpscAsyncQueue(std::size_t capacity)
      : cap_(roundUpPow2(capacity < 2 ? 2 : capacity)),
        mask_(cap_ - 1),
        low_water_(cap_ / 2),
        ring_(cap_) {}

  SpscAsyncQueue(const SpscAsyncQueue&)            = delete;
  SpscAsyncQueue& operator=(const SpscAsyncQueue&) = delete;

  // Producer side. Returns false iff requestStop() was called.
  bool push(T value) {
    const std::uint64_t t = tail_.load(std::memory_order_relaxed);
    std::uint64_t       h = head_.load(std::memory_order_acquire);

    // Hysteresis: if we observe a full ring, park until size <= low_water.
    if (t - h >= cap_) {
      while (true) {
        if (stop_.load(std::memory_order_acquire)) return false;

        h = head_.load(std::memory_order_acquire);
        if (t - h <= low_water_) break;

        if (stop_.load(std::memory_order_acquire)) return false;
        // Re-check under the token snapshot. Any consumer bump or stop
        // after this load advances the counter; wait() catches it.
        const std::uint64_t token = producer_wakeup_.load(std::memory_order_acquire);
        producer_wakeup_.wait(token, std::memory_order_acquire);

        h = head_.load(std::memory_order_acquire);
        if (t - h <= low_water_) break;
      }
      if (stop_.load(std::memory_order_acquire)) return false;
    }

    ring_[t & mask_] = std::move(value);
    tail_.store(t + 1, std::memory_order_release);

    // Fast-path optimization: only bump + notify when the consumer is
    // currently parked (or about to park). The consumer sets `waiting_`
    // before it commits to wait() and clears it after the wait returns.
    // Reading the flag is one acquire load — cheaper than fetch_add per
    // push when the consumer is keeping up.
    if (consumer_waiting_.load(std::memory_order_acquire) != 0) {
      consumer_wakeup_.fetch_add(1, std::memory_order_release);
      consumer_wakeup_.notify_one();
    }
    return true;
  }

  // Consumer side. Returns false when (empty AND closed) or stop requested.
  bool pop(T& out) {
    std::uint64_t h = head_.load(std::memory_order_relaxed);
    while (true) {
      const std::uint64_t t = tail_.load(std::memory_order_acquire);
      if (h < t) {
        out = std::move(ring_[h & mask_]);
        head_.store(h + 1, std::memory_order_release);
        // Hysteresis notify: only when we've just brought size to the
        // low-water mark. If the producer is parked it's waiting for
        // exactly this signal.
        const std::uint64_t new_size = t - (h + 1);
        if (new_size == low_water_) {
          producer_wakeup_.fetch_add(1, std::memory_order_release);
          producer_wakeup_.notify_one();
        }
        return true;
      }
      // Empty. Set the waiting flag *before* snapshotting the wake counter
      // so the producer's load of waiting_ in push() is guaranteed to see
      // 1 if it ever observes our wait. Then snapshot, re-check state, and
      // park. If a close()/requestStop()/push() lands between the snapshot
      // and the wait(), the counter has advanced and wait() returns
      // immediately.
      consumer_waiting_.store(1, std::memory_order_release);
      if (closed_.load(std::memory_order_acquire) ||
          stop_.load(std::memory_order_acquire)) {
        consumer_waiting_.store(0, std::memory_order_release);
        return false;
      }
      const std::uint64_t token =
          consumer_wakeup_.load(std::memory_order_acquire);
      const std::uint64_t t2 = tail_.load(std::memory_order_acquire);
      if (h < t2) {
        consumer_waiting_.store(0, std::memory_order_release);
        continue;
      }
      if (closed_.load(std::memory_order_acquire) ||
          stop_.load(std::memory_order_acquire)) {
        consumer_waiting_.store(0, std::memory_order_release);
        return false;
      }
      consumer_wakeup_.wait(token, std::memory_order_acquire);
      consumer_waiting_.store(0, std::memory_order_release);
    }
  }

  void close() {
    closed_.store(true, std::memory_order_release);
    consumer_wakeup_.fetch_add(1, std::memory_order_release);
    consumer_wakeup_.notify_all();
  }

  void requestStop() {
    stop_.store(true, std::memory_order_release);
    producer_wakeup_.fetch_add(1, std::memory_order_release);
    producer_wakeup_.notify_all();
    consumer_wakeup_.fetch_add(1, std::memory_order_release);
    consumer_wakeup_.notify_all();
  }

  std::size_t capacity() const noexcept { return cap_; }

  std::size_t size() const noexcept {
    const std::uint64_t t = tail_.load(std::memory_order_acquire);
    const std::uint64_t h = head_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(t - h);
  }

  bool closed() const noexcept {
    return closed_.load(std::memory_order_acquire);
  }

 private:
  static std::size_t roundUpPow2(std::size_t n) noexcept {
    return std::bit_ceil(n);
  }

  // Producer-owned cache line.
  alignas(kCacheLine) std::atomic<std::uint64_t> tail_{0};
  // Consumer-owned cache line.
  alignas(kCacheLine) std::atomic<std::uint64_t> head_{0};
  // Producer parks here when full; consumer/stop bump.
  alignas(kCacheLine) std::atomic<std::uint64_t> producer_wakeup_{0};
  // Consumer parks here when empty; producer/close/stop bump.
  alignas(kCacheLine) std::atomic<std::uint64_t> consumer_wakeup_{0};
  // Set by consumer before wait, cleared after. Producer skips notify when 0.
  alignas(kCacheLine) std::atomic<int>           consumer_waiting_{0};
  // Lifecycle flags on their own line.
  alignas(kCacheLine) std::atomic<bool> closed_{false};
  std::atomic<bool>                     stop_{false};

  std::size_t cap_;
  std::size_t mask_;
  std::size_t low_water_;

  std::vector<T> ring_;
};

}  // namespace cmf
