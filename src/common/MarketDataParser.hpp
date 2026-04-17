#pragma once

#include "MarketDataEvent.hpp"
#include <optional>
#include <string>

std::optional<MarketDataEvent> parseLine(const std::string &line);