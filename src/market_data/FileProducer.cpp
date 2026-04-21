#include "market_data/FileProducer.hpp"

#include "market_data/MmapMboSource.hpp"

#include <utility>

namespace cmf {

FileProducer::FileProducer(std::filesystem::path path,
                           std::size_t queue_capacity)
    : path_(std::move(path)),
      queue_(std::make_unique<SpscRingQueue<MarketDataEvent>>(
          queue_capacity)) {}

FileProducer::~FileProducer() {
  try {
    join();
  } catch (...) {
    // Swallow: dtor must be noexcept-equivalent.
  }
}

void FileProducer::start() { thread_ = std::thread(&FileProducer::run, this); }

void FileProducer::join() {
  if (thread_.joinable())
    thread_.join();
}

void FileProducer::run() {
  try {
    MmapMboSource source(path_);
    MarketDataEvent event;
    std::uint64_t n = 0;
    while (source.next(event)) {
      // Backpressure: spin-yield while the consumer drains.
      while (!queue_->push(std::move(event)))
        std::this_thread::yield();
      ++n;
    }
    lines_.store(n, std::memory_order_release);
  } catch (...) {
    error_ = std::current_exception();
  }
  queue_->close();
}

} // namespace cmf
