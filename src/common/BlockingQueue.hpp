#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace cmf {

template <typename T>
class BlockingQueue {
public:
    explicit BlockingQueue(std::size_t capacity) : capacity_(capacity) {}

    void push(T value) {
        std::unique_lock lock(mu_);
        not_full_.wait(lock, [this] { return queue_.size() < capacity_; });
        queue_.push(std::move(value));
        not_empty_.notify_one();
    }

    T pop() {
        std::unique_lock lock(mu_);
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
        return value;
    }

private:
    std::queue<T>           queue_;
    std::mutex              mu_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::size_t             capacity_;
};

} // namespace cmf
