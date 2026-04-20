#pragma once

#include "Parser.hpp"
#include "data_ingestion/MarketDataEvent.hpp"

namespace cmf::parser{
    class JSONParser: public MDParser {
        MarketDataEvent parse_line(const std::string & line) override;
    };
}