#pragma once

#include "QueueBase.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <utility>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define CMF_CPU_RELAX() _mm_pause()
#elif defined(__aarch64__) || defined(__arm64__)
#define CMF_CPU_RELAX() __asm__ volatile("yield" ::: "memory")
#else
#include <thread>
#define CMF_CPU_RELAX() std::this_thread::yield()
#endif

namespace cmf
{

// SPSC lock-free ring buffer.
//
// Invariants:
//   head_ : next slot to WRITE  (producer-owned)
//   tail_ : next slot to READ   (consumer-owned)
//   Buffer is FULL  when (head_ + 1) & MASK == tail_
//   Buffer is EMPTY when head_ == tail_
//
// Memory ordering rationale:
//   Producer writes head_ with release  → consumer acquires head_
//   Consumer writes tail_ with release  → producer acquires tail_
//   closed_ store uses seq_cst so the flag is visible to spinning threads on
//   weakly-ordered ARM without an extra fence; acquire loads are sufficient
//   to synchronize-with that seq_cst store on all platforms.
//
// Cached-index optimization (rigtorp-style):
//   tail_cached_ lives on the producer cache line beside head_.
//   head_cached_ lives on the consumer cache line beside tail_.
//   In the fast path (queue neither full nor empty) each thread touches only
//   its own cache line — no cross-cache-line atomic traffic.
template <typename T, std::size_t Capacity = 8192>
class LockFreeQueue : public QueueBase<LockFreeQueue<T, Capacity>, T>
{
    friend class QueueBase<LockFreeQueue<T, Capacity>, T>;

  public:
    LockFreeQueue() = default;
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
    LockFreeQueue(LockFreeQueue&&) = delete;
    LockFreeQueue& operator=(LockFreeQueue&&) = delete;
    ~LockFreeQueue() = default;

  private:
    void push_impl(T value);
    void push_batch_impl(T* items, std::size_t count);

    template <typename Fn>
        requires std::invocable<Fn, T&&>
    [[nodiscard]] bool pop_impl(Fn&& fn);

    void close_impl() noexcept;

    [[nodiscard]] bool is_closed_impl() const noexcept;
    [[nodiscard]] bool empty_impl() const noexcept;
    [[nodiscard]] std::size_t size_impl() const noexcept;

    static constexpr std::size_t MASK = Capacity - 1;
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of 2");

    std::array<T, Capacity> buffer_;

    // Producer cache line: head_ (owned) + stale snapshot of tail_.
    // tail_cached_ is non-atomic: only ever read/written by the producer thread.
    alignas(64) std::atomic<std::size_t> head_{0};
    std::size_t tail_cached_{0};

    // Consumer cache line: tail_ (owned) + stale snapshot of head_.
    // head_cached_ is non-atomic: only ever read/written by the consumer thread.
    alignas(64) std::atomic<std::size_t> tail_{0};
    std::size_t head_cached_{0};

    alignas(64) std::atomic<bool> closed_{false};
};

// ---------------------------------------------------------------------------
// push_impl
// ---------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
void LockFreeQueue<T, Capacity>::push_impl(T value)
{
    if (closed_.load(std::memory_order_acquire)) [[unlikely]]
        throw std::runtime_error("LockFreeQueue::push: queue is closed");

    // head_ is producer-private; load once — it never changes under our feet.
    const std::size_t h = head_.load(std::memory_order_relaxed);
    const std::size_t next_h = (h + 1) & MASK;

    // Fast path: check stale tail snapshot (no atomic op).
    // Slow path: spin refreshing tail_cached_ from the real tail_.
    if (next_h == tail_cached_) [[unlikely]]
    {
        do
        {
            if (closed_.load(std::memory_order_acquire)) [[unlikely]]
                throw std::runtime_error("LockFreeQueue::push: queue is closed");
            CMF_CPU_RELAX();
            tail_cached_ = tail_.load(std::memory_order_acquire);
        } while (next_h == tail_cached_);
    }

    buffer_[h] = std::move(value);
    head_.store(next_h, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// push_batch_impl
//
// Reserves space for the entire batch up front, then writes all slots with no
// atomics, and publishes with a single release store.  This is O(count) with
// one CAS-free publication vs. the previous N individual stores.
// ---------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
void LockFreeQueue<T, Capacity>::push_batch_impl(T* items, std::size_t count)
{
    if (closed_.load(std::memory_order_acquire)) [[unlikely]]
        throw std::runtime_error("LockFreeQueue::push_batch: queue is closed");
    if (count == 0)
        return;
    if (count >= Capacity) [[unlikely]]
        throw std::runtime_error(
            "LockFreeQueue::push_batch: count must be less than Capacity");

    const std::size_t h = head_.load(std::memory_order_relaxed);

    // Free slots = (tail_cached_ - h - 1 + Capacity) & MASK.
    // Spin, refreshing the cached tail, until enough contiguous slots are free.
    while (((tail_cached_ - h - 1 + Capacity) & MASK) < count)
    {
        if (closed_.load(std::memory_order_acquire)) [[unlikely]]
            throw std::runtime_error("LockFreeQueue::push_batch: queue is closed");
        CMF_CPU_RELAX();
        tail_cached_ = tail_.load(std::memory_order_acquire);
    }

    for (std::size_t i = 0; i < count; ++i)
        buffer_[(h + i) & MASK] = std::move(items[i]);

    // Single release store makes the entire batch visible to the consumer.
    head_.store((h + count) & MASK, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// pop_impl
// ---------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
template <typename Fn>
    requires std::invocable<Fn, T&&>
bool LockFreeQueue<T, Capacity>::pop_impl(Fn&& fn)
{
    // tail_ is consumer-private; load once.
    const std::size_t t = tail_.load(std::memory_order_relaxed);

    // Fast path: check stale head snapshot (no atomic op).
    if (t == head_cached_) [[unlikely]]
    {
        // Slow path: refresh head cache.
        head_cached_ = head_.load(std::memory_order_acquire);
        if (t == head_cached_)
        {
            // Truly empty — spin until an item arrives or the queue closes.
            do
            {
                if (closed_.load(std::memory_order_acquire)) [[unlikely]]
                {
                    // A push-then-close may have happened between the head_ load above
                    // and this closed_ load; re-check before giving up.
                    head_cached_ = head_.load(std::memory_order_acquire);
                    if (t == head_cached_)
                        return false;
                    break; // item slipped in just before close — consume it
                }
                CMF_CPU_RELAX();
                head_cached_ = head_.load(std::memory_order_acquire);
            } while (t == head_cached_);
        }
    }

    T item = std::move(buffer_[t]);

    // Advance tail_ before invoking fn so the producer slot is freed as early
    // as possible.  fn must be noexcept-safe: the item is already moved-from.
    tail_.store((t + 1) & MASK, std::memory_order_release);
    std::forward<Fn>(fn)(std::move(item));
    return true;
}

// ---------------------------------------------------------------------------
// close_impl
// ---------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
void LockFreeQueue<T, Capacity>::close_impl() noexcept
{
    // seq_cst store creates a total-order fence so spinning threads on
    // weakly-ordered ARM/POWER see the flag without needing an extra barrier.
    closed_.store(true, std::memory_order_seq_cst);
}

// ---------------------------------------------------------------------------
// Observers
// ---------------------------------------------------------------------------
template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::is_closed_impl() const noexcept
{
    // acquire is sufficient: synchronizes-with the seq_cst store in close_impl.
    return closed_.load(std::memory_order_acquire);
}

template <typename T, std::size_t Capacity>
bool LockFreeQueue<T, Capacity>::empty_impl() const noexcept
{
    return tail_.load(std::memory_order_acquire) ==
           head_.load(std::memory_order_acquire);
}

template <typename T, std::size_t Capacity>
std::size_t LockFreeQueue<T, Capacity>::size_impl() const noexcept
{
    const std::size_t h = head_.load(std::memory_order_acquire);
    const std::size_t t = tail_.load(std::memory_order_acquire);
    // Unsigned wrap-around makes (h - t) & MASK correct for all h, t pairs.
    return (h - t) & MASK;
}

} // namespace cmf
