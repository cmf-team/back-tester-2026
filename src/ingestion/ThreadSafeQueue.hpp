#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class ThreadSafeQueue
{
  public:
    explicit ThreadSafeQueue(std::size_t capacity = 512) : capacity_{capacity} {}

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue(ThreadSafeQueue&&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(ThreadSafeQueue&&) = delete;

    void Push(T value)
    {
        std::unique_lock<std::mutex> lock{mtx_};
        cv_not_full_.wait(lock, [this]
                          { return q_.size() < capacity_ || done_; });
        if (!done_)
        {
            q_.push(std::move(value));
            cv_not_empty_.notify_one();
        }
    }

    auto BlockingPop() -> std::optional<T>
    {
        std::unique_lock<std::mutex> lock{mtx_};
        cv_not_empty_.wait(lock, [this]
                           { return !q_.empty() || done_; });
        if (q_.empty())
            return std::nullopt;
        T val{std::move(q_.front())};
        q_.pop();
        cv_not_full_.notify_one();
        return val;
    }

    void MarkDone()
    {
        std::lock_guard<std::mutex> lock{mtx_};
        done_ = true;
        cv_not_empty_.notify_all();
        cv_not_full_.notify_all();
    }

    auto Size() const -> std::size_t
    {
        std::lock_guard<std::mutex> lock{mtx_};
        return q_.size();
    }

    auto IsDone() const -> bool
    {
        std::lock_guard<std::mutex> lock{mtx_};
        return done_;
    }

  private:
    std::queue<T> q_;
    mutable std::mutex mtx_;
    std::condition_variable cv_not_empty_;
    std::condition_variable cv_not_full_;
    std::size_t capacity_;
    bool done_{false};
};
