#pragma once
#include <atomic>
#include <array>
#include <thread>
#include <cstddef>
#include <immintrin.h>

namespace cmf {

template <typename T, std::size_t Cap = 1024>
class SpscQueue {
    static_assert((Cap & (Cap - 1)) == 0, "Cap must be a power of 2");
    static constexpr std::size_t MASK = Cap - 1;

    struct alignas(64) ProdSide {
        std::atomic<std::size_t> tail{0};
        std::size_t              cached_head{0};
        char                     pad[64 - sizeof(std::atomic<std::size_t>) - sizeof(std::size_t)];
    };

    struct alignas(64) ConsSide {
        std::atomic<std::size_t> head{0};
        std::size_t              cached_tail{0};
        char                     pad[64 - sizeof(std::atomic<std::size_t>) - sizeof(std::size_t)];
    };

    alignas(64) std::array<T, Cap> buf_{};
    alignas(64) ProdSide           p_{};
    alignas(64) ConsSide           c_{};

public:
    void push(const T& item) noexcept {
        const std::size_t t = p_.tail.load(std::memory_order_relaxed);
        if (t - p_.cached_head >= Cap) {
            int spins = 0;
            while (t - (p_.cached_head = c_.head.load(std::memory_order_acquire)) >= Cap) {
                if (++spins < 64)
                    _mm_pause();
                else {
                    spins = 0;
                    std::this_thread::yield();
                }
            }
        }
        buf_[t & MASK] = item;
        p_.tail.store(t + 1, std::memory_order_release);
    }

    void pop(T& out) noexcept {
        const std::size_t h = c_.head.load(std::memory_order_relaxed);
        if (h >= c_.cached_tail) {
            int spins = 0;
            while (h >= (c_.cached_tail = p_.tail.load(std::memory_order_acquire))) {
                if (++spins < 64)
                    _mm_pause();
                else {
                    spins = 0;
                    std::this_thread::yield();
                }
            }
        }
        out = buf_[h & MASK];
        c_.head.store(h + 1, std::memory_order_release);
    }

    T pop() noexcept {
        T out;
        pop(out);
        return out;
    }
};

} // namespace cmf