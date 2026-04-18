// Abstract source of MarketDataEvents. Pulled one event at a time.
// Implementations: MarketDataParser (single file), ChainedMarketDataSource
// (N files), and in principle live feeds / test fakes.

#pragma once

#include "parser/MarketDataEvent.hpp"

namespace cmf {

class IMarketDataSource {
public:
  virtual ~IMarketDataSource() = default;

  // Populates `out` with the next event. Returns false when the source is
  // exhausted. Behaviour on partial/malformed input is implementation-defined.
  virtual bool next(MarketDataEvent& out) = 0;
};

} // namespace cmf
