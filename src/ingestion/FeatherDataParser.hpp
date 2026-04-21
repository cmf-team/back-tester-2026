#pragma once

#include "DataParser.hpp"
#include <functional>

namespace cmf {
class FeatherDataParser : public DataParser<FeatherDataParser> {
  friend class DataParser<FeatherDataParser>;
  void parse_inner(const std::function<void(const MarketDataEvent &)> &f) const;

public:
  static constexpr std::string_view filename_ext = ".mbo.json.feather";
  using DataParser<FeatherDataParser>::DataParser;
};
} // namespace cmf
