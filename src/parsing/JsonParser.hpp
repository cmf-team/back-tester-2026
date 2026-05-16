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

std::uint64_t parseTimestampText(std::string_view text);
std::uint64_t parseUInt64Text(std::string_view text);
std::int64_t parsePriceText(std::string_view text);
Side parseSideText(std::string_view text);
Action parseActionText(std::string_view text);

}
