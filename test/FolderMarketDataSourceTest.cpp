// tests for FolderMarketDataSource

#include "parser/FolderMarketDataSource.hpp"

#include "catch2/catch_all.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace cmf;

namespace {

// Two valid lines — different instrument_ids so we can distinguish them.
constexpr const char* kLineA =
    R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":100},"action":"A","side":"B","price":"0.021200000","size":20,"channel_id":79,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":1,"symbol":"AAA"})"
    "\n";

constexpr const char* kLineB =
    R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":200},"action":"A","side":"B","price":"0.021200000","size":20,"channel_id":79,"order_id":"2","flags":0,"ts_in_delta":0,"sequence":2,"symbol":"BBB"})"
    "\n";

void writeFile(const std::filesystem::path& p, const std::string& contents) {
  std::ofstream ofs(p, std::ios::binary);
  ofs.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

class TempDir {
public:
  explicit TempDir(const std::string& name)
      : path_(std::filesystem::temp_directory_path() / name) {
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }
  ~TempDir() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const noexcept { return path_; }

private:
  std::filesystem::path path_;
};

} // namespace

TEST_CASE("FolderMarketDataSource concatenates *.mbo.json files in order",
          "[FolderMarketDataSource]") {
  TempDir dir("folder_mds_chain");
  // Names chosen so alphabetical sort is deterministic.
  writeFile(dir.path() / "20260309.aaa.mbo.json",
            std::string(kLineA) + kLineB);
  writeFile(dir.path() / "20260310.bbb.mbo.json", std::string(kLineA));
  writeFile(dir.path() / "20260311.ccc.mbo.json", std::string(kLineB));

  FolderMarketDataSource src(dir.path());
  REQUIRE(src.files().size() == 3u);

  std::vector<uint32_t> ids;
  MarketDataEvent       ev;
  while (src.next(ev)) ids.push_back(ev.instrument_id);

  REQUIRE(ids == std::vector<uint32_t>{100, 200, 100, 200});
  REQUIRE(src.currentFileIndex() == 3u);
}

TEST_CASE("FolderMarketDataSource ignores non-matching files",
          "[FolderMarketDataSource]") {
  TempDir dir("folder_mds_filter");
  writeFile(dir.path() / "condition.json", "{\"ignore\":true}");
  writeFile(dir.path() / "manifest.json", "{\"ignore\":true}");
  writeFile(dir.path() / "metadata.json", "{\"ignore\":true}");
  writeFile(dir.path() / "20260309.data.mbo.json", std::string(kLineA));

  FolderMarketDataSource src(dir.path());
  REQUIRE(src.files().size() == 1u);

  MarketDataEvent ev;
  REQUIRE(src.next(ev));
  REQUIRE(ev.instrument_id == 100u);
  REQUIRE_FALSE(src.next(ev));
}

TEST_CASE("FolderMarketDataSource with empty folder returns false immediately",
          "[FolderMarketDataSource]") {
  TempDir                 dir("folder_mds_empty");
  FolderMarketDataSource  src(dir.path());
  MarketDataEvent         ev;
  REQUIRE(src.files().empty());
  REQUIRE_FALSE(src.next(ev));
}

TEST_CASE("FolderMarketDataSource throws on non-directory input",
          "[FolderMarketDataSource]") {
  REQUIRE_THROWS(FolderMarketDataSource(
      std::filesystem::path("/definitely/does/not/exist/xyz")));
}
