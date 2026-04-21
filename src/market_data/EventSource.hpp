// EventSource — abstraction over one stream of chronologically-sorted events.
//
// Implementations:
//   * SpscQueueSource — wraps a SpscRingQueue<MarketDataEvent> fed by a
//     producer thread (real ingestion path).
//   * VectorEventSource — drains a pre-populated std::vector (unit tests).
//
// The pop() method blocks until either an event is available (returns true)
// or the underlying stream is permanently exhausted (returns false).

#pragma once

#include "market_data/MarketDataEvent.hpp"
#include "market_data/SpscRingQueue.hpp"

#include <thread>
#include <utility>
#include <vector>

namespace cmf {

class IEventSource {
public:
  IEventSource() = default;
  virtual ~IEventSource() = default;

  IEventSource(const IEventSource &) = delete;
  IEventSource &operator=(const IEventSource &) = delete;

  // Blocks until an event is produced, or until the upstream signals done.
  // Returns true and sets `out` on success, false at end-of-stream.
  virtual bool pop(MarketDataEvent &out) = 0;
};

// Wraps an SPSC ring queue: spins with yield until either an item becomes
// available or the producer closes the queue and the queue drains.
class SpscQueueSource final : public IEventSource {
public:
  explicit SpscQueueSource(SpscRingQueue<MarketDataEvent> &q) noexcept
      : q_(&q) {}

  bool pop(MarketDataEvent &out) override {
    while (true) {
      if (q_->pop(out))
        return true;
      if (q_->done())
        return false;
      std::this_thread::yield();
    }
  }

private:
  SpscRingQueue<MarketDataEvent> *q_;
};

// Drains a pre-populated vector in index order. Non-blocking.
class VectorEventSource final : public IEventSource {
public:
  explicit VectorEventSource(std::vector<MarketDataEvent> items) noexcept
      : items_(std::move(items)) {}

  bool pop(MarketDataEvent &out) override {
    if (idx_ >= items_.size())
      return false;
    out = std::move(items_[idx_]);
    ++idx_;
    return true;
  }

private:
  std::vector<MarketDataEvent> items_;
  std::size_t idx_{0};
};

} // namespace cmf
