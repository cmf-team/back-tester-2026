#pragma once

#include "MarketDataEvent.hpp"

#include <optional>
#include <string>

namespace cmf {

std::optional<MarketDataEvent> parseLine(const std::string& line);

}