#pragma once

#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"

#include <deque>

namespace cmf {
template <typename Derived,
          template <typename> typename InputQueueImpl = BlockingQueue,
          template <typename> typename OutputQueueImpl = InputQueueImpl>
  requires QueueType<InputQueueImpl<MarketDataEvent>, MarketDataEvent> &&
           QueueType<OutputQueueImpl<MarketDataEvent>, MarketDataEvent>
class Merger {
public:
  using InputQueue = InputQueueImpl<MarketDataEvent>;
  using OutputQueue = OutputQueueImpl<MarketDataEvent>;

  Merger(std::deque<InputQueue> &file_queues, OutputQueue &output)
      : file_queues_(file_queues), output_(output) {}

  void run() { static_cast<Derived *>(this)->run_impl(); }

protected:
  std::deque<InputQueue> &file_queues_;
  OutputQueue &output_;
};
} // namespace cmf
