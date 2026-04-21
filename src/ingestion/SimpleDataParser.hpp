#pragma once

#include "DataParser.hpp"
#include <functional>

namespace cmf {
class SimpleDataParser : public DataParser<SimpleDataParser> {
  friend class DataParser<SimpleDataParser>;
  void parse_inner(const std::function<void(const MarketDataEvent &)> &f) const;

public:
  using DataParser<SimpleDataParser>::DataParser;
};
} // namespace cmf
