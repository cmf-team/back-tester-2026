#pragma once
#include "data_ingestion/MarketDataEvent.hpp"
#include <string>

namespace cmf::parser{
class MDParser {
    public:
        virtual ~MDParser() = default;

        virtual MarketDataEvent parse_line(const std::string & line) = 0;
};
}