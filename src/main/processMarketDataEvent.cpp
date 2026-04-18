#include "main/processMarketDataEvent.hpp"

#include "main/EventCollector.hpp"

namespace cmf {

void processMarketDataEvent(const MarketDataEvent &order) {
  defaultEventCollector()(order);
}

} // namespace cmf
