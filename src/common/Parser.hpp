#pragma once

#include <string>
#include "MarketDataEvent.hpp"
#include "json.hpp"

namespace cmf {

using json = nlohmann::json;

std::string getStr(const json& j, const std::string& key, const std::string& def = "");

MarketDataEvent parseLine(const std::string& line);

}