#pragma once

#include "domain/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace md {

MarketDataEvent parseMarketDataEventLine(std::string_view line, std::size_t line_number);

MarketDataEvent parseMarketDataEventLine(
    std::string_view line,
    std::size_t line_number,
    std::uint32_t source_file_id,
    std::uint64_t source_sequence
);

}
