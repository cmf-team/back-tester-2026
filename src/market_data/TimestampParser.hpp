// TimestampParser — ISO 8601 with 9-digit fractional seconds, UTC only.

#pragma once

#include "common/BasicTypes.hpp"

#include <string_view>

namespace cmf {

// Parses a timestamp of the exact Databento pretty_ts format:
//   "YYYY-MM-DDTHH:MM:SS.fffffffffZ"  (30 characters, UTC, 9-digit ns)
// into nanoseconds since the UNIX epoch.
// Throws std::invalid_argument on malformed input.
NanoTime parseDatabentoTimestamp(std::string_view s);

} // namespace cmf
