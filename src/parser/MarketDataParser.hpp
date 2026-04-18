// Positional JSONL parser for Databento MBO market-data.
// Assumes the fixed key order produced by XEUR/EOBI batch downloads.

#pragma once

#include "parser/MarketDataEvent.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>

namespace cmf {

// RAII wrapper around a read-only mmap (Linux + macOS).
class MappedFile {
public:
  explicit MappedFile(const std::filesystem::path& path);
  ~MappedFile();

  MappedFile(const MappedFile&)            = delete;
  MappedFile& operator=(const MappedFile&) = delete;
  MappedFile(MappedFile&& other) noexcept;
  MappedFile& operator=(MappedFile&& other) noexcept;

  const char*      data() const noexcept { return data_; }
  std::size_t      size() const noexcept { return size_; }
  std::string_view view() const noexcept { return {data_, size_}; }

private:
  const char* data_{nullptr};
  std::size_t size_{0};
  int         fd_{-1};
};

class MarketDataParser {
public:
  // Opens and mmaps the file.
  explicit MarketDataParser(const std::filesystem::path& path);

  // Parses from an externally-owned buffer (primarily for unit tests).
  MarketDataParser(const char* begin, const char* end) noexcept;

  // Parses the next line into `out`. Returns false at EOF.
  // On malformed input behaviour is undefined - intended for trusted vendor data.
  bool next(MarketDataEvent& out);

  std::size_t bytesConsumed() const noexcept;
  std::size_t totalBytes() const noexcept;

private:
  std::unique_ptr<MappedFile> file_;
  const char*                 begin_{nullptr};
  const char*                 cur_{nullptr};
  const char*                 end_{nullptr};
};

} // namespace cmf
