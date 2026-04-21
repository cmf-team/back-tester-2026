#pragma once

#include "BasicTypes.hpp"

namespace cmf {

struct MarketDataEvent {
  NanoTime timestamp;
  OrderId order_id;
  Side side;
  Price price;
  Quantity size;
  Action action;
};

} // namespace cmf