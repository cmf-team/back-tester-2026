#pragma once

#include "common/SpscQueue.hpp"
#include "common/MarketDataEvent.hpp"

namespace cmf {

using EventQueue = SpscQueue<MarketDataEvent>;

} // namespace cmf
