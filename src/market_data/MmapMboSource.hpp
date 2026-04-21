// MmapMboSource — opens one Databento MBO NDJSON file via mmap and yields
// events one at a time. Composition of MappedFile + PositionalMboParser.

#pragma once

#include "market_data/MappedFile.hpp"
#include "market_data/MarketDataEvent.hpp"
#include "market_data/PositionalMboParser.hpp"

#include <filesystem>

namespace cmf {

class MmapMboSource {
public:
  // Opens and mmaps the file. Validates that the content starts with the
  // expected Databento prefix `{"ts_recv":"` so the positional parser
  // doesn't silently produce nonsense when the feed format changes.
  // Empty files are allowed (zero events). Throws std::runtime_error on
  // I/O failure or unexpected prefix.
  explicit MmapMboSource(const std::filesystem::path &path);

  MmapMboSource(const MmapMboSource &) = delete;
  MmapMboSource &operator=(const MmapMboSource &) = delete;

  // Parses and returns the next event. Returns false at EOF.
  bool next(MarketDataEvent &out) { return parser_.next(out); }

  const std::filesystem::path &path() const noexcept { return path_; }
  std::size_t size() const noexcept { return file_.size(); }

private:
  std::filesystem::path path_;
  MappedFile file_;
  PositionalMboParser parser_;
};

} // namespace cmf
