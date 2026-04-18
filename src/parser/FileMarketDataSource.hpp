// FileMarketDataSource: streams MarketDataEvent objects out of one daily
// NDJSON L3 (Databento MBO) file. Reads line by line so that even
// multi-gigabyte files are processed with a bounded memory footprint.

#pragma once

#include "parser/MarketDataEvent.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace cmf {

// Result of parsing a single NDJSON line.
enum class ParseStatus {
  Ok,           // line successfully parsed into MarketDataEvent
  Empty,        // blank line - skip
  Malformed,    // could not parse - skip & report
};

// Parses one JSON line into `out`. Pure function; primarily used by
// FileMarketDataSource and unit tests.
ParseStatus parseMarketDataLine(std::string_view line, MarketDataEvent &out);

class FileMarketDataSource {
public:
  explicit FileMarketDataSource(const std::filesystem::path &path);

  // Reads the next valid event from the file. Returns false at EOF.
  // Lines that are empty or malformed are silently skipped, but their count
  // is exposed via skippedCount() / malformedCount().
  bool next(MarketDataEvent &out);

  std::uint64_t lineNumber() const noexcept { return line_no_; }
  std::uint64_t skippedCount() const noexcept { return skipped_; }
  std::uint64_t malformedCount() const noexcept { return malformed_; }

  const std::filesystem::path &path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
  std::ifstream stream_;
  std::string line_buf_;
  std::uint64_t line_no_{0};
  std::uint64_t skipped_{0};
  std::uint64_t malformed_{0};
};

} // namespace cmf
