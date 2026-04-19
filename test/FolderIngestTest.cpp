#include "ingest/FolderIngest.hpp"
#include "common/MarketDataEvent.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

using namespace cmf;

namespace {

class TempDir {
public:
  explicit TempDir(std::string_view name)
      : path_{std::filesystem::temp_directory_path() / std::string{name}} {
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
  }

  ~TempDir() { std::filesystem::remove_all(path_); }

  const std::filesystem::path &getPath() const { return path_; }

private:
  std::filesystem::path path_;
};

void writeLines(const std::filesystem::path &path,
                const std::vector<std::string> &lines) {
  std::ofstream os(path);
  for (const auto &line : lines) {
    os << line << '\n';
  }
}

std::vector<std::uint64_t>
collectStrategyTimestamps(const std::filesystem::path &folder,
                          MergeStrategy strategy, FolderIngestStats &stats,
                          FolderIngestOptions options = {}) {
  std::vector<std::uint64_t> seen;
  stats = ingestFolder(
      folder, strategy,
      [&](const MarketDataEvent &event) { seen.push_back(event.order_id); },
      options);
  return seen;
}

} // namespace

TEST_CASE("ingestFolder preserves chronological order for both strategies",
          "[ingest][folder]") {
  TempDir dir("folder_ingest_order");
  writeLines(
      dir.getPath() / "a.json",
      {
          R"({"ts_recv":100,"hd":{"ts_event":100,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"1001","action":"A","side":"B","price":1,"size":1,"sequence":1})",
          R"({"ts_recv":103,"hd":{"ts_event":103,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"1002","action":"A","side":"B","price":1,"size":1,"sequence":2})",
          R"({"ts_recv":106,"hd":{"ts_event":106,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"1003","action":"A","side":"B","price":1,"size":1,"sequence":3})",
      });
  writeLines(
      dir.getPath() / "b.json",
      {
          R"({"ts_recv":101,"hd":{"ts_event":101,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"2001","action":"A","side":"B","price":1,"size":1,"sequence":4})",
          R"({"ts_recv":104,"hd":{"ts_event":104,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"2002","action":"A","side":"B","price":1,"size":1,"sequence":5})",
      });
  writeLines(
      dir.getPath() / "c.json",
      {
          R"({"ts_recv":102,"hd":{"ts_event":102,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"3001","action":"A","side":"B","price":1,"size":1,"sequence":6})",
          R"({"ts_recv":105,"hd":{"ts_event":105,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"3002","action":"A","side":"B","price":1,"size":1,"sequence":7})",
          R"({"ts_recv":107,"hd":{"ts_event":107,"rtype":160,"publisher_id":1,"instrument_id":1},"order_id":"3003","action":"A","side":"B","price":1,"size":1,"sequence":8})",
      });

  FolderIngestStats flat_stats{};
  const FolderIngestOptions tiny_batches{.queue_capacity = 2, .batch_size = 2};
  const auto flat_order = collectStrategyTimestamps(
      dir.getPath(), MergeStrategy::Flat, flat_stats, tiny_batches);
  FolderIngestStats hierarchy_stats{};
  const auto hierarchy_order = collectStrategyTimestamps(
      dir.getPath(), MergeStrategy::Hierarchy, hierarchy_stats, tiny_batches);

  const std::vector<std::uint64_t> expected{
      1001, 2001, 3001, 1002, 2002, 3002, 1003, 3003};
  REQUIRE(flat_order == expected);
  REQUIRE(hierarchy_order == expected);

  REQUIRE(flat_stats.files == 3);
  REQUIRE(flat_stats.total == expected.size());
  REQUIRE(flat_stats.skipped_rtype == 0);
  REQUIRE(flat_stats.skipped_parse == 0);
  REQUIRE(flat_stats.out_of_order_ts_recv == 0);
  REQUIRE(flat_stats.first_ts_recv == 100);
  REQUIRE(flat_stats.last_ts_recv == 107);

  REQUIRE(hierarchy_stats.files == 3);
  REQUIRE(hierarchy_stats.total == expected.size());
  REQUIRE(hierarchy_stats.skipped_rtype == 0);
  REQUIRE(hierarchy_stats.skipped_parse == 0);
  REQUIRE(hierarchy_stats.out_of_order_ts_recv == 0);
}

TEST_CASE("ingestFolder keeps deterministic order for equal event keys",
          "[ingest][folder]") {
  TempDir dir("folder_ingest_equal_keys");
  writeLines(
      dir.getPath() / "a.json",
      {
          R"({"ts_recv":500,"hd":{"ts_event":500,"rtype":160,"publisher_id":7,"instrument_id":1},"order_id":"11","action":"A","side":"B","price":1,"size":1,"sequence":9})",
      });
  writeLines(
      dir.getPath() / "b.json",
      {
          R"({"ts_recv":500,"hd":{"ts_event":500,"rtype":160,"publisher_id":7,"instrument_id":1},"order_id":"22","action":"A","side":"B","price":1,"size":1,"sequence":9})",
      });

  FolderIngestStats flat_stats{};
  const FolderIngestOptions tiny_batches{.queue_capacity = 1, .batch_size = 1};
  const auto flat_order = collectStrategyTimestamps(
      dir.getPath(), MergeStrategy::Flat, flat_stats, tiny_batches);
  FolderIngestStats hierarchy_stats{};
  const auto hierarchy_order = collectStrategyTimestamps(
      dir.getPath(), MergeStrategy::Hierarchy, hierarchy_stats, tiny_batches);

  const std::vector<std::uint64_t> expected{11, 22};
  REQUIRE(flat_order == expected);
  REQUIRE(hierarchy_order == expected);
}

TEST_CASE("ingestFolder rejects zero batch_size and zero queue_capacity",
          "[folder-ingest]") {
  TempDir dir("back-tester-folder-ingest-zero-opts");
  writeLines(
      dir.getPath() / "a.json",
      {
          R"({"ts_recv":100,"hd":{"ts_event":100,"rtype":160,"publisher_id":7,"instrument_id":1},"order_id":"1","action":"A","side":"B","price":1,"size":1,"sequence":1})",
      });

  const auto consumer = [](const MarketDataEvent &) {};

  REQUIRE_THROWS_AS(
      ingestFolder(dir.getPath(), MergeStrategy::Flat, consumer,
                   FolderIngestOptions{.queue_capacity = 0}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(ingestFolder(dir.getPath(), MergeStrategy::Flat, consumer,
                                 FolderIngestOptions{.batch_size = 0}),
                    std::invalid_argument);
}
