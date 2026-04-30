#pragma once

#include "MarketDataEvent.hpp"
#include <optional>
#include <string>

auto parseNDJSON(const std::string& line) -> std::optional<MarketDataEvent>;

auto nanosToIso(std::uint64_t ns) -> std::string;