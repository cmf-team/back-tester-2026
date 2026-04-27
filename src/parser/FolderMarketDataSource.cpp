#include "parser/FolderMarketDataSource.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace cmf {

namespace {

constexpr std::string_view kMboSuffix = ".mbo.json";

bool hasSuffix(const std::string& s, std::string_view suf) noexcept {
  return s.size() >= suf.size()
      && std::equal(suf.rbegin(), suf.rend(), s.rbegin());
}

std::vector<std::filesystem::path>
collectFiles(const std::filesystem::path& folder) {
  if (!std::filesystem::is_directory(folder))
    throw std::runtime_error("not a directory: " + folder.string());

  std::vector<std::filesystem::path> out;
  for (const auto& entry : std::filesystem::directory_iterator(folder)) {
    if (!entry.is_regular_file()) continue;
    if (hasSuffix(entry.path().filename().string(), kMboSuffix))
      out.push_back(entry.path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

} // namespace

FolderMarketDataSource::FolderMarketDataSource(
    const std::filesystem::path& folder)
    : files_(collectFiles(folder)) {}

bool FolderMarketDataSource::openNext() {
  cur_.reset();
  if (idx_ < files_.size()) {
    cur_ = std::make_unique<FileMarketDataSource>(files_[idx_]);
    return true;
  }
  return false;
}

bool FolderMarketDataSource::next(MarketDataEvent& out) {
  for (;;) {
    if (!cur_) {
      if (idx_ >= files_.size()) return false;
      if (!openNext()) return false;
    }
    if (cur_->next(out)) return true;

    cur_.reset();
    ++idx_;
  }
}

} // namespace cmf
