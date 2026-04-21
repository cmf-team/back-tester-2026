#include "market_data/MmapMboSource.hpp"

#include <cstring>
#include <stdexcept>
#include <string_view>

namespace cmf {

namespace {

// Every Databento MBO record from XEUR/EOBI starts with this prefix. Checking
// it once per file converts silent parser UB (on format changes) into a loud
// runtime error.
constexpr std::string_view kExpectedPrefix = R"({"ts_recv":")";

} // namespace

MmapMboSource::MmapMboSource(const std::filesystem::path &path)
    : path_(path), file_(path) {
  if (file_.size() == 0) {
    // Empty file — legal (no events). Parser over null range returns false.
    parser_.reset(nullptr, nullptr);
    return;
  }

  if (file_.size() < kExpectedPrefix.size() ||
      std::memcmp(file_.data(), kExpectedPrefix.data(),
                  kExpectedPrefix.size()) != 0) {
    throw std::runtime_error(
        "MmapMboSource: unexpected file prefix (not Databento MBO?) for '" +
        path.string() + "'");
  }

  parser_.reset(file_.data(), file_.data() + file_.size());
}

} // namespace cmf
