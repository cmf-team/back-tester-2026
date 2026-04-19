#pragma once

#include "MarketDataEvent.hpp"
#include <optional>
#include <string>

std::optional<MarketDataEvent> parseNDJSON(const std::string &line);