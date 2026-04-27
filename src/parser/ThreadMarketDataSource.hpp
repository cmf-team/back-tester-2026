// Threaded wrapper around any duck-typed market-data source.
//
// Spawns a producer thread that pulls events from `Upstream` and pushes them
// into an AsyncQueue. The wrapped source can be a FileMarketDataSource,
// FolderMarketDataSource, VariantSource, or any merger — anything that
// exposes `bool next(MarketDataEvent&)`. The wrapper itself exposes the same
// duck-typed contract, so it can sit anywhere in the pipeline.
//
// Backpressure is delegated to AsyncQueue: producer parks when full, wakes
// when consumer drains to capacity/2. See AsyncQueue.hpp.

#pragma once

#include "common/AsyncQueue.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstddef>
#include <thread>
#include <utility>

namespace cmf {

template <class Upstream>
class ThreadMarketDataSource {
 public:
  ThreadMarketDataSource(Upstream& upstream, std::size_t buffer_capacity)
      : upstream_(upstream), queue_(buffer_capacity) {
    thread_ = std::thread([this] { producerLoop(); });
  }

  ~ThreadMarketDataSource() {
    queue_.requestStop();
    if (thread_.joinable()) thread_.join();
  }

  ThreadMarketDataSource(const ThreadMarketDataSource&)            = delete;
  ThreadMarketDataSource& operator=(const ThreadMarketDataSource&) = delete;
  ThreadMarketDataSource(ThreadMarketDataSource&&)                 = delete;
  ThreadMarketDataSource& operator=(ThreadMarketDataSource&&)      = delete;

  bool next(MarketDataEvent& out) { return queue_.pop(out); }

  std::size_t bufferCapacity() const noexcept { return queue_.capacity(); }

 private:
  void producerLoop() {
    MarketDataEvent ev;
    while (upstream_.next(ev)) {
      if (!queue_.push(std::move(ev))) return;  // consumer aborted
    }
    queue_.close();
  }

  Upstream&                   upstream_;
  AsyncQueue<MarketDataEvent> queue_;
  std::thread                 thread_;
};

}  // namespace cmf
