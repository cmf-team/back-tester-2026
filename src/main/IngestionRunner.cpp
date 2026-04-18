#include "main/IngestionRunner.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace cmf {

IngestionRunner::IngestionRunner(std::filesystem::path path, Consumer consumer)
    : path_(std::move(path)), consumer_(std::move(consumer)) {
  if (!consumer_)
    throw std::invalid_argument("IngestionRunner: consumer must not be empty");
}

IngestionStats IngestionRunner::run() {
  FileMarketDataSource source(path_);
  MarketDataEvent ev;

  const auto t0 = std::chrono::steady_clock::now();
  std::uint64_t total = 0;
  while (source.next(ev)) {
    consumer_(ev);
    ++total;
  }
  const auto t1 = std::chrono::steady_clock::now();

  IngestionStats stats;
  stats.path = path_;
  stats.total_events = total;
  stats.skipped_lines = source.skippedCount();
  stats.malformed_lines = source.malformedCount();
  stats.elapsed_seconds = std::chrono::duration<double>(t1 - t0).count();
  return stats;
}

} // namespace cmf
