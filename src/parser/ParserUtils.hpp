#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cmf {

inline constexpr int64_t UNDEF_PRICE = INT64_MAX;
inline constexpr uint64_t UNDEF_TIMESTAMP = UINT64_MAX;

using MarketDataEventCallback = std::function<void(const MarketDataEvent &)>;

// Parse an NDJSON file at `path` and append every successfully parsed event to
// `events`. If `onEvent` is provided, it is invoked for each event right after
// it is appended (used for streaming diagnostics). Malformed lines are skipped.
void parseNdjsonFile(const std::string &path,
                     std::vector<MarketDataEvent> &events,
                     const MarketDataEventCallback &onEvent = {});

} // namespace cmf
