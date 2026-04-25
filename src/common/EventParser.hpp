#pragma once

#include "common/MarketDataEvent.hpp"

#include <optional>
#include <string_view>

namespace cmf {
    std::optional<MarketDataEvent> parseEvent(std::string_view line);


    NanoTime parseIsoTimestamp(std::string_view iso);


    std::int64_t parseFixedPrice(std::string_view str);
} // namespace cmf
