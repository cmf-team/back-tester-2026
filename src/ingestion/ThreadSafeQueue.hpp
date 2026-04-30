#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

template <typename T>
class SPSCQueue
{
  public:
    explicit SPSCQueue(std::size_t capacity = 4096) : capacity_{capacity}, buffer_{capacity} {}

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    void Push(T value)
    {
        std::unique_lock<std::mutex> lock{mtx_};

        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next_tail = (tail + 1) % capacity_;

        cv_not_full_.wait(lock, [this, next_tail]
                          { return next_tail != head_.load(std::memory_order_acquire) || done_; });

        if (done_.load(std::memory_order_acquire))
            return;

        buffer_[tail] = std::move(value);
        tail_.store(next_tail, std::memory_order_release);

        lock.unlock();
        cv_not_empty_.notify_one();
    }

    void PushBatch(const std::vector<T>& values)
    {
        std::unique_lock<std::mutex> lock{mtx_};

        for (const auto& value : values)
        {
            std::size_t tail = tail_.load(std::memory_order_relaxed);
            std::size_t next_tail = (tail + 1) % capacity_;

            cv_not_full_.wait(lock, [this, next_tail]
                              { return next_tail != head_.load(std::memory_order_acquire) || done_; });

            if (done_.load(std::memory_order_acquire))
                return;

            buffer_[tail] = value;
            tail_.store(next_tail, std::memory_order_release);
        }

        lock.unlock();
        cv_not_empty_.notify_one();
    }

    auto BlockingPop() -> std::optional<T>
    {
        std::unique_lock<std::mutex> lock{mtx_};

        cv_not_empty_.wait(lock, [this]
                           { return head_.load(std::memory_order_relaxed) !=
                                        tail_.load(std::memory_order_acquire) ||
                                    done_.load(std::memory_order_acquire); });

        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);

        if (head == tail)
            return std::nullopt;

        T val{std::move(buffer_[head])};
        head_.store((head + 1) % capacity_, std::memory_order_release);

        lock.unlock();
        cv_not_full_.notify_one();

        return val;
    }

    auto PopBatch(std::vector<T>& out, std::size_t max_count) -> std::size_t
    {
        std::unique_lock<std::mutex> lock{mtx_};

        cv_not_empty_.wait(lock, [this]
                           { return head_.load(std::memory_order_relaxed) !=
                                        tail_.load(std::memory_order_acquire) ||
                                    done_.load(std::memory_order_acquire); });

        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);

        if (head == tail)
            return 0;

        std::size_t available = (tail - head + capacity_) % capacity_;
        std::size_t to_pop = std::min(available, max_count);

        out.clear();
        out.reserve(to_pop);

        for (std::size_t i = 0; i < to_pop; ++i)
        {
            std::size_t idx = (head + i) % capacity_;
            out.push_back(std::move(buffer_[idx]));
        }

        head_.store((head + to_pop) % capacity_, std::memory_order_release);

        lock.unlock();
        cv_not_full_.notify_one();

        return to_pop;
    }

    void MarkDone()
    {
        std::lock_guard<std::mutex> lock{mtx_};
        done_.store(true, std::memory_order_release);
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }

    auto Size() const -> std::size_t
    {
        std::lock_guard<std::mutex> lock{mtx_};
        auto head = head_.load(std::memory_order_acquire);
        auto tail = tail_.load(std::memory_order_acquire);
        return (tail - head + capacity_) % capacity_;
    }

    auto IsDone() const -> bool
    {
        return done_.load(std::memory_order_acquire);
    }

  private:
    std::size_t capacity_;
    std::vector<T> buffer_;

    mutable std::mutex mtx_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;

    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
    alignas(64) std::atomic<bool> done_{false};
};

template <typename T>
using ThreadSafeQueue = SPSCQueue<T>;
