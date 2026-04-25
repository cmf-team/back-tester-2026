#pragma once
#include <atomic>
#include <array>
#include <thread>
#include <cstddef>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace cmf {

template <typename T, std::size_t Cap = 1024>
class SpscQueue {
    static_assert((Cap & (Cap - 1)) == 0, "Capacity must be power of 2");
    static constexpr std::size_t IndexMask = Cap - 1;

    struct alignas(64) Writer {
        std::atomic<std::size_t> writeIdx{0};
        std::size_t              readCache{0};
    };

    struct alignas(64) Reader {
        std::atomic<std::size_t> readIdx{0};
        std::size_t              writeCache{0};
    };

    alignas(64) std::array<T, Cap> storage_{};
    alignas(64) Writer             writer_{};
    alignas(64) Reader             reader_{};

public:
    void push(const T& item) noexcept {
        std::size_t write = writer_.writeIdx.load(std::memory_order_relaxed);

        // Fast path
        if (write - writer_.readCache >= Cap) {
            slow_push(write);
        }

        storage_[write & IndexMask] = item;
        writer_.writeIdx.store(write + 1, std::memory_order_release);
    }

    void pop(T& out) noexcept {
        std::size_t read = reader_.readIdx.load(std::memory_order_relaxed);

        // Fast path
        if (reader_.writeCache - read == 0) {
            slow_pop(read);
        }

        out = storage_[read & IndexMask];
        reader_.readIdx.store(read + 1, std::memory_order_release);
    }

private:
    void slow_push(std::size_t& write) noexcept {
        int attempts = 0;

        for (;;) {
            writer_.readCache = reader_.readIdx.load(std::memory_order_acquire);

            if (write - writer_.readCache < Cap) {
                return;
            }

            backoff(attempts);
        }
    }

    void slow_pop(std::size_t& read) noexcept {
        int attempts = 0;

        for (;;) {
            reader_.writeCache = writer_.writeIdx.load(std::memory_order_acquire);

            if (reader_.writeCache - read > 0) {
                return;
            }

            backoff(attempts);
        }
    }

    static inline void backoff(int& attempts) noexcept {
        if (attempts < 48) {
#if defined(__x86_64__) || defined(__i386__)
            _mm_pause();
#else
            std::this_thread::yield();
#endif
        } else {
            std::this_thread::yield();
            attempts = 0;
        }
        ++attempts;
    }
};

} // namespace cmf