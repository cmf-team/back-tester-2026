// QueueEventSource: adapts a producer-side EventQueue to the pull-based
// IEventSource interface used by mergers. End-of-stream is signalled by
// the queue being closed and drained (pop() returning std::nullopt).

#pragma once

#include "pipeline/EventQueue.hpp"
#include "pipeline/IEventSource.hpp"

#include <memory>

namespace cmf {

class QueueEventSource final : public IEventSource {
public:
  explicit QueueEventSource(std::shared_ptr<EventQueue<MarketDataEvent>> q)
      : q_(std::move(q)) {}

  bool next(MarketDataEvent &out) override {
    auto v = q_->pop();
    if (!v)
      return false;
    out = std::move(*v);
    return true;
  }

private:
  std::shared_ptr<EventQueue<MarketDataEvent>> q_;
};

} // namespace cmf
