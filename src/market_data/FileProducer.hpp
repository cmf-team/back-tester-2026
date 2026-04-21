// FileProducer — one thread: file -> parser -> SpscRingQueue.

#pragma once

#include "market_data/MarketDataEvent.hpp"
#include "market_data/SpscRingQueue.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <thread>

namespace cmf {

class FileProducer {
public:
  // Default queue capacity — 64Ki slots. Tune via ctor if producer/consumer
  // throughput mismatch causes sustained backpressure or idle consumer.
  static constexpr std::size_t kDefaultQueueCapacity = 64 * 1024;

  explicit FileProducer(
      std::filesystem::path path,
      std::size_t queue_capacity = kDefaultQueueCapacity);

  // The destructor joins the thread if still running. Throws nothing.
  ~FileProducer();

  FileProducer(const FileProducer &) = delete;
  FileProducer &operator=(const FileProducer &) = delete;

  // Launches the reader/parser thread. Call exactly once.
  void start();

  // Joins the thread (safe to call multiple times).
  void join();

  // Parsed-and-enqueued line counter (exact once join() returns).
  std::uint64_t linesProduced() const noexcept {
    return lines_.load(std::memory_order_acquire);
  }

  // Any exception thrown on the producer thread. Null if clean.
  std::exception_ptr error() const noexcept { return error_; }

  SpscRingQueue<MarketDataEvent> &queue() noexcept { return *queue_; }
  const std::filesystem::path &path() const noexcept { return path_; }

private:
  void run();

  std::filesystem::path path_;
  std::unique_ptr<SpscRingQueue<MarketDataEvent>> queue_;
  std::thread thread_;
  std::atomic<std::uint64_t> lines_{0};
  std::exception_ptr error_;
};

} // namespace cmf
