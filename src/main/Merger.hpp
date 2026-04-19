#pragma once

#include "MarketDataEvent.hpp"
#include "ThreadSafeQueue.hpp"

#include <functional>
#include <vector>


namespace cmf {

size_t flatMerge(
    std::vector<ThreadSafeQueue<MarketDataEvent>>& queues,
    std::function<void(const MarketDataEvent&)> consumer
);

size_t hierarchyMerge(
    std::vector<ThreadSafeQueue<MarketDataEvent>>& queues,
    std::function<void(const MarketDataEvent&)> consumer
);

} // namespace cmf