#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <thread>
#include <immintrin.h>

namespace cmf {

// head_ and tail_ on separate cache lines to prevent false sharing.
template <typename T, std::size_t Cap = 4096>
class SpscQueue {
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be a power of 2");
    static constexpr std::size_t MASK = Cap - 1;

    struct alignas(64) Atom { std::atomic<std::size_t> v{0}; };

    alignas(64) std::array<T, Cap> buf_{};
    Atom head_{}; // consumer advances
    Atom tail_{}; // producer advances

public:
    void push(const T& item) noexcept {
        const std::size_t t = tail_.v.load(std::memory_order_relaxed);
        int spins = 0;
        while (t - head_.v.load(std::memory_order_acquire) == Cap) {
            if (++spins < 64) _mm_pause();
            else { spins = 0; std::this_thread::yield(); }
        }
        buf_[t & MASK] = item;
        tail_.v.store(t + 1, std::memory_order_release);
    }

    T pop() noexcept {
        const std::size_t h = head_.v.load(std::memory_order_relaxed);
        int spins = 0;
        while (tail_.v.load(std::memory_order_acquire) == h) {
            if (++spins < 64) _mm_pause();
            else { spins = 0; std::this_thread::yield(); }
        }
        T item = buf_[h & MASK];
        head_.v.store(h + 1, std::memory_order_release);
        return item;
    }
};

} // namespace cmf
