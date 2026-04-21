#pragma once

#include "DataParser.hpp"
#include <functional>

namespace cmf {
class NativeDataParser : public DataParser<NativeDataParser> {
  friend class DataParser<NativeDataParser>;
  void parse_inner(const std::function<void(const MarketDataEvent &)> &f) const;

public:
  using DataParser<NativeDataParser>::DataParser;
};
} // namespace cmf
