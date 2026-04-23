// Iterates every *.mbo.json file under a folder as a single IMarketDataSource.
// Files are sorted alphabetically (chronological for yyyymmdd names) and opened
// lazily, one at a time, so only a single mmap is live.

#pragma once

#include "parser/FileMarketDataSource.hpp"
#include "parser/IMarketDataSource.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace cmf {

class FolderMarketDataSource final : public IMarketDataSource {
public:
  explicit FolderMarketDataSource(const std::filesystem::path& folder);

  bool next(MarketDataEvent& out) override;

  const std::vector<std::filesystem::path>& files() const noexcept {
    return files_;
  }

  // Index of the file currently being parsed (0-based); equals files().size()
  // once the source is exhausted.
  std::size_t currentFileIndex() const noexcept { return idx_; }

private:
  bool openNext();

  std::vector<std::filesystem::path> files_;
  std::size_t                        idx_{0};
  std::unique_ptr<FileMarketDataSource>  cur_;
};

} // namespace cmf
