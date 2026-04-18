#pragma once

#include <string>
#include "MarketDataEvent.h"

class JsonLineParser {
public:
    static MarketDataEvent parse(const std::string& line);
};