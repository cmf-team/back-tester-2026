// IEventSource: tiny pull-based interface that everything in the merge
// pipeline speaks. A source is "exhausted" once next() returns false.
//
// Why an abstract interface (and not "vector<EventQueue*>" everywhere):
//   * Mergers themselves are sources, so they compose: a HierarchyMerger
//     can take a FlatMerger as one of its leaves, etc.
//   * Unit tests inject a VectorEventSource and exercise all of the merge
//     logic synchronously, without spinning up real producer threads.
//
// IEventSource is single-consumer; it does not need to be thread-safe.

#pragma once

#include "parser/MarketDataEvent.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace cmf {

class IEventSource {
public:
  virtual ~IEventSource() = default;
  // Read the next event. Returns false at end-of-stream.
  virtual bool next(MarketDataEvent &out) = 0;
};

using EventSourcePtr = std::unique_ptr<IEventSource>;

// In-memory source that yields a fixed vector of events in order.
// Mostly useful for tests and for benchmarks that want to isolate merger
// behaviour from JSON parsing cost.
class VectorEventSource final : public IEventSource {
public:
  explicit VectorEventSource(std::vector<MarketDataEvent> events) noexcept
      : events_(std::move(events)) {}

  bool next(MarketDataEvent &out) override {
    if (cursor_ >= events_.size())
      return false;
    out = std::move(events_[cursor_++]);
    return true;
  }

  std::size_t consumed() const noexcept { return cursor_; }
  std::size_t remaining() const noexcept { return events_.size() - cursor_; }

private:
  std::vector<MarketDataEvent> events_;
  std::size_t cursor_{0};
};

} // namespace cmf
