#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <thread>
#include <immintrin.h>

namespace cmf {

// Producer and consumer each cache the opposite end's index to avoid touching
// the contended atomic on every operation. The atomic is only re-read when the
// cached value suggests the queue is full (producer) or empty (consumer).
template <typename T, std::size_t Cap = 16384>
class SpscQueue {
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be a power of 2");
    static constexpr std::size_t MASK = Cap - 1;

    struct alignas(64) ProdSide {
        std::atomic<std::size_t> tail{0};
        std::size_t              cached_head{0};
    };
    struct alignas(64) ConsSide {
        std::atomic<std::size_t> head{0};
        std::size_t              cached_tail{0};
    };

    alignas(64) std::array<T, Cap> buf_{};
    ProdSide p_{};
    ConsSide c_{};

public:
    void push(const T& item) noexcept {
        const std::size_t t = p_.tail.load(std::memory_order_relaxed);
        if (t - p_.cached_head == Cap) {
            int spins = 0;
            while (t - (p_.cached_head = c_.head.load(std::memory_order_acquire)) == Cap) {
                if (++spins < 64) _mm_pause();
                else { spins = 0; std::this_thread::yield(); }
            }
        }
        buf_[t & MASK] = item;
        p_.tail.store(t + 1, std::memory_order_release);
    }

    T pop() noexcept {
        T out;
        pop(out);
        return out;
    }

    void pop(T& out) noexcept {
        const std::size_t h = c_.head.load(std::memory_order_relaxed);
        if (h == c_.cached_tail) {
            int spins = 0;
            while (h == (c_.cached_tail = p_.tail.load(std::memory_order_acquire))) {
                if (++spins < 64) _mm_pause();
                else { spins = 0; std::this_thread::yield(); }
            }
        }
        out = buf_[h & MASK];
        c_.head.store(h + 1, std::memory_order_release);
    }
};

} // namespace cmf
