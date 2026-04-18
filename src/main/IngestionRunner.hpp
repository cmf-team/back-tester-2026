// IngestionRunner: drives one daily NDJSON file through a user-supplied
// consumer and returns aggregate ingestion statistics. The runner does not
// know how to print anything - that is Reporting's job.

#pragma once

#include "parser/FileMarketDataSource.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>

namespace cmf {

struct IngestionStats {
  std::filesystem::path path;
  std::uint64_t total_events{0};
  std::uint64_t skipped_lines{0};
  std::uint64_t malformed_lines{0};
  double elapsed_seconds{0.0};
};

class IngestionRunner {
public:
  using Consumer = std::function<void(const MarketDataEvent &)>;

  IngestionRunner(std::filesystem::path path, Consumer consumer);

  IngestionStats run();

  const std::filesystem::path &path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
  Consumer consumer_;
};

} // namespace cmf
