#pragma once

#include "../Parser.hpp"
#include "common/MarketDataEvent.hpp"

namespace cmf::parser{
    class JSONParser: public MDParser {
        MarketDataEvent parse_line(const std::string & line) override;
    };
}