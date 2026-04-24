#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

// Thread-safe blocking queue used between producers and the dispatcher.
// Producers call push(); dispatcher calls pop() which blocks until data
// is available or the queue is marked done.
template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(std::size_t max_size = 200'000)
        : max_size_(max_size) {}

    // Producer side — blocks if queue is full
    void push(T item) {
        std::unique_lock lock(mu_);
        cv_full_.wait(lock, [this]{ return queue_.size() < max_size_ || done_; });
        if (done_) return;
        queue_.push(std::move(item));
        cv_empty_.notify_one();
    }

    // Consumer side — blocks until item available or done
    std::optional<T> pop() {
        std::unique_lock lock(mu_);
        cv_empty_.wait(lock, [this]{ return !queue_.empty() || done_; });
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        cv_full_.notify_one();
        return item;
    }

    // Call once all producers are finished
    void set_done() {
        { std::lock_guard lock(mu_); done_ = true; }
        cv_empty_.notify_all();
        cv_full_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard lock(mu_);
        return queue_.size();
    }

    bool is_done() const {
        std::lock_guard lock(mu_);
        return done_ && queue_.empty();
    }

private:
    mutable std::mutex      mu_;
    std::condition_variable cv_empty_, cv_full_;
    std::queue<T>           queue_;
    std::size_t             max_size_;
    bool                    done_{false};
};
