// NDJSON → MarketDataEvent reader.
//
// parseNdjsonFile is stateless and reentrant: each call owns its ifstream,
// JSON parser, and local stats. Safe to run N concurrent instances on N
// different files, as the hard variant will require.

#pragma once

#include "common/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>

namespace cmf {

struct IngestStats {
  std::size_t consumed = 0;
  std::size_t skipped_rtype = 0;
  std::size_t skipped_parse = 0;
  std::size_t out_of_order_ts_recv = 0;
  std::uint64_t first_ts_recv = UNDEF_TIMESTAMP;
  std::uint64_t last_ts_recv = UNDEF_TIMESTAMP;
};

using MarketDataEventConsumer = std::function<void(const MarketDataEvent &)>;
using MarketDataEventVisitor = std::function<bool(const MarketDataEvent &)>;

IngestStats parseNdjsonFile(const std::filesystem::path &path,
                            const MarketDataEventVisitor &visitor);

IngestStats parseNdjsonFile(const std::filesystem::path &path,
                            const MarketDataEventConsumer &consumer);

} // namespace cmf
