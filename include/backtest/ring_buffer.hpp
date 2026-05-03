#pragma once

#include <memory>
#include <array>
#include <cstdint>
#include <algorithm>
#include <type_traits>

namespace backtest {

template<typename T, size_t CAPACITY>
class RingBuffer {
public:
    static_assert(CAPACITY > 0, "Capacity must be > 0");
    static_assert((CAPACITY & (CAPACITY - 1)) == 0,
                  "Capacity must be power of 2 for fast modulo");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T must be trivially copyable for lock-free operation");

    constexpr RingBuffer() noexcept
        : buffer_(std::make_unique<std::array<T, CAPACITY>>()) {}

    ~RingBuffer() noexcept = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    RingBuffer(RingBuffer&&) noexcept = default;
    RingBuffer& operator=(RingBuffer&&) noexcept = default;

    void push(const T& item) noexcept {
        const size_t index = write_index_ & (CAPACITY - 1);  // Fast modulo
        (*buffer_)[index] = item;
        write_index_++;

        if (total_count_ < CAPACITY) {
            total_count_++;
        } else {
            read_index_ = write_index_ - CAPACITY + 1;
        }
    }

    template<typename Func>
    void forEach(Func&& func) const noexcept {
        const size_t count = std::min(total_count_, CAPACITY);
        const size_t start = read_index_ & (CAPACITY - 1);

        for (size_t i = 0; i < count; ++i) {
            const size_t index = (start + i) & (CAPACITY - 1);
            func((*buffer_)[index]);
        }
    }

    [[nodiscard]] size_t size() const noexcept {
        return std::min(total_count_, CAPACITY);
    }

    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return CAPACITY;
    }

    [[nodiscard]] bool empty() const noexcept {
        return total_count_ == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return total_count_ >= CAPACITY;
    }

    void clear() noexcept {
        write_index_ = 0;
        read_index_ = 0;
        total_count_ = 0;
    }

    [[nodiscard]] const T& operator[](size_t index) const noexcept {
        return (*buffer_)[(read_index_ + index) & (CAPACITY - 1)];
    }

private:
    std::unique_ptr<std::array<T, CAPACITY>> buffer_;
    size_t write_index_ = 0;
    size_t read_index_ = 0;
    size_t total_count_ = 0;
};

}