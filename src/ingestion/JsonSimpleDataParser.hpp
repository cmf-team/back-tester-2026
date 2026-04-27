#pragma once

#include "DataParser.hpp"
#include <functional>

namespace cmf
{
class JsonSimpleDataParser : public DataParser<JsonSimpleDataParser>
{
    friend class DataParser<JsonSimpleDataParser>;
    void parse_inner(const std::function<void(const MarketDataEvent&)>& f) const;

  public:
    static constexpr std::string_view filename_ext = ".mbo.json";
    using DataParser<JsonSimpleDataParser>::DataParser;
};
} // namespace cmf
