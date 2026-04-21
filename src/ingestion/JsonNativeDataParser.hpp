#pragma once

#include "DataParser.hpp"
#include <functional>

namespace cmf {
class JsonNativeDataParser : public DataParser<JsonNativeDataParser> {
  friend class DataParser<JsonNativeDataParser>;
  void parse_inner(const std::function<void(const MarketDataEvent &)> &f) const;

public:
  static constexpr std::string_view bench_suffix = ".mbo.json";
  using DataParser<JsonNativeDataParser>::DataParser;
};
} // namespace cmf
