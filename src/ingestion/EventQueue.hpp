#pragma once

#include "common/BlockingQueue.hpp"
#include "common/MarketDataEvent.hpp"
#include <optional>

namespace cmf {

using EventQueue = BlockingQueue<std::optional<MarketDataEvent>>;

} // namespace cmf
