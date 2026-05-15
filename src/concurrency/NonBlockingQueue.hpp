#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace md {

template <typename T>
class NonBlockingQueue {
public:
    explicit NonBlockingQueue(std::size_t batch_size = 256)
        : batch_size_(batch_size), head_(new BatchNode), tail_(head_) {
        if (batch_size_ == 0) {
            throw std::invalid_argument("NonBlockingQueue batch size must be greater than zero");
        }

        producer_batch_.reserve(batch_size_);
        consumer_batch_.reserve(batch_size_);
    }

    ~NonBlockingQueue() {
        BatchNode* node = head_;
        while (node != nullptr) {
            BatchNode* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    NonBlockingQueue(const NonBlockingQueue&) = delete;
    NonBlockingQueue& operator=(const NonBlockingQueue&) = delete;

    void push(T value) {
        producer_batch_.push_back(std::move(value));
        enqueued_count_.fetch_add(1, std::memory_order_relaxed);

        if (producer_batch_.size() >= batch_size_) {
            flush();
        }
    }

    void flush() {
        if (producer_batch_.empty()) {
            return;
        }

        auto node = std::make_unique<BatchNode>();
        node->items.swap(producer_batch_);
        producer_batch_.reserve(batch_size_);

        BatchNode* raw_node = node.release();
        tail_->next.store(raw_node, std::memory_order_release);
        tail_->next.notify_one();
        tail_ = raw_node;
    }

    T pop() {
        if (consumer_index_ >= consumer_batch_.size()) {
            acquireConsumerBatch();
        }

        dequeued_count_.fetch_add(1, std::memory_order_relaxed);
        return std::move(consumer_batch_[consumer_index_++]);
    }

    [[nodiscard]] std::size_t size() const {
        const std::size_t enqueued = enqueued_count_.load(std::memory_order_relaxed);
        const std::size_t dequeued = dequeued_count_.load(std::memory_order_relaxed);
        return enqueued >= dequeued ? enqueued - dequeued : 0;
    }

private:
    struct BatchNode {
        std::vector<T> items;
        std::atomic<BatchNode*> next{nullptr};
    };

    void acquireConsumerBatch() {
        consumer_batch_.clear();
        consumer_index_ = 0;

        BatchNode* next = head_->next.load(std::memory_order_acquire);
        while (next == nullptr) {
            head_->next.wait(nullptr, std::memory_order_acquire);
            next = head_->next.load(std::memory_order_acquire);
        }

        consumer_batch_.swap(next->items);
        BatchNode* old_head = head_;
        head_ = next;
        delete old_head;
    }

    const std::size_t batch_size_;

    BatchNode* head_;
    BatchNode* tail_;

    std::vector<T> producer_batch_;
    std::vector<T> consumer_batch_;
    std::size_t consumer_index_{0};

    alignas(64) std::atomic<std::size_t> enqueued_count_{0};
    alignas(64) std::atomic<std::size_t> dequeued_count_{0};
};

} // namespace md
