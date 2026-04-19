#pragma once

#include "LockFreeQueue.hpp"
#include "MarketDataEvent.hpp"
#include "MergeStrategies.hpp"
#include "NativeDataParser.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace cmf {
namespace EventParser {

// Producer thread that reads and parses a file
class FileProducer {
private:
  std::string filepath_;
  LockFreeQueue<MarketDataEvent> *queue_;
  std::atomic<bool> *done_;
  std::atomic<size_t> *messages_read_;

public:
  FileProducer(const std::string &filepath,
               LockFreeQueue<MarketDataEvent> *queue, std::atomic<bool> *done,
               std::atomic<size_t> *messages_read)
      : filepath_(filepath), queue_(queue), done_(done),
        messages_read_(messages_read) {}

  void run() {
    std::ifstream file(filepath_);
    if (!file.is_open()) {
      done_->store(true, std::memory_order_release);
      return;
    }

    std::string line;
    line.reserve(512); // Pre-allocate typical line size

    while (std::getline(file, line)) {
      if (line.empty()) {
        continue;
      }

      MarketDataEvent event;
      if (NativeDataParser::parse(line, event)) {
        // Try to push, spin if queue is full
        while (!queue_->tryPush(std::move(event))) {
          std::this_thread::yield();
        }
        messages_read_->fetch_add(1, std::memory_order_relaxed);
      }
    }

    done_->store(true, std::memory_order_release);
  }
};

// Optimized ingestion pipeline with producer-consumer pattern
template <typename MergerType> class IngestionPipeline {
private:
  std::vector<std::string> filepaths_;
  std::vector<std::unique_ptr<LockFreeQueue<MarketDataEvent>>> queues_;
  std::vector<std::unique_ptr<FileProducer>> producers_;
  std::vector<std::thread> producer_threads_;
  std::vector<std::atomic<bool>> done_flags_;
  std::vector<std::atomic<size_t>> messages_read_;

  size_t total_messages_ = 0;

public:
  explicit IngestionPipeline(const std::vector<std::string> &filepaths)
      : filepaths_(filepaths), done_flags_(filepaths.size()),
        messages_read_(filepaths.size()) {
    // Initialize queues and producers
    for (size_t i = 0; i < filepaths_.size(); ++i) {
      queues_.push_back(std::make_unique<LockFreeQueue<MarketDataEvent>>());
      done_flags_[i].store(false, std::memory_order_relaxed);
      messages_read_[i].store(0, std::memory_order_relaxed);

      producers_.push_back(std::make_unique<FileProducer>(
          filepaths_[i], queues_[i].get(), &done_flags_[i],
          &messages_read_[i]));
    }
  }

  // Run the pipeline with dispatcher thread merging events
  template <typename ProcessFunc>
  void run(ProcessFunc &&processEvent, bool warmup = false) {
    // Start producer threads
    for (auto &producer : producers_) {
      producer_threads_.emplace_back([&producer]() { producer->run(); });
    }

    // Dispatcher: merge events from all queues in chronological order
    std::vector<std::optional<MarketDataEvent>> pending_events(
        filepaths_.size());
    size_t active_producers = filepaths_.size();

    while (active_producers > 0) {
      // Find the event with minimum timestamp
      size_t min_idx = filepaths_.size();
      NanoTime min_timestamp = std::numeric_limits<NanoTime>::max();

      for (size_t i = 0; i < filepaths_.size(); ++i) {
        // Try to fetch next event from this queue if we don't have one pending
        if (!pending_events[i].has_value()) {
          pending_events[i] = queues_[i]->tryPop();
        }

        // Check if this event is the minimum
        if (pending_events[i].has_value()) {
          if (pending_events[i]->timestamp < min_timestamp) {
            min_timestamp = pending_events[i]->timestamp;
            min_idx = i;
          }
        } else if (done_flags_[i].load(std::memory_order_acquire) &&
                   queues_[i]->empty()) {
          // This producer is done
          if (done_flags_[i].exchange(false, std::memory_order_acq_rel)) {
            active_producers--;
          }
        }
      }

      // Process the minimum event
      if (min_idx < filepaths_.size()) {
        if (!warmup) {
          processEvent(pending_events[min_idx].value());
        }
        total_messages_++;
        pending_events[min_idx].reset();
      } else {
        // No events available, yield
        std::this_thread::yield();
      }
    }

    // Wait for all producer threads to complete
    for (auto &thread : producer_threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  size_t getTotalMessages() const { return total_messages_; }

  // Get statistics
  std::vector<size_t> getMessagesPerFile() const {
    std::vector<size_t> counts;
    for (const auto &counter : messages_read_) {
      counts.push_back(counter.load(std::memory_order_relaxed));
    }
    return counts;
  }
};

// Simplified batch-based ingestion (alternative approach)
template <typename MergerType> class BatchIngestionPipeline {
private:
  MergerType merger_;
  size_t batch_size_;

public:
  explicit BatchIngestionPipeline(const std::vector<std::string> &filepaths,
                                  size_t batch_size = 1024)
      : merger_(filepaths), batch_size_(batch_size) {}

  template <typename ProcessFunc> size_t run(ProcessFunc &&processEvent) {
    size_t total = 0;
    std::vector<MarketDataEvent> batch;
    batch.reserve(batch_size_);

    while (merger_.hasNext()) {
      batch.clear();

      // Fetch a batch
      while (batch.size() < batch_size_ && merger_.hasNext()) {
        batch.push_back(merger_.getNext());
      }

      // Process batch
      for (const auto &event : batch) {
        processEvent(event);
        total++;
      }
    }

    return total;
  }
};

} // namespace EventParser
} // namespace cmf
