// Integration tests for the Hard-task pipeline (FileProducer + merger +
// dispatcher). Uses three small synthetic NDJSON files.

#include "market_data/HardTask.hpp"

#include "TempFile.hpp"
#include "catch2/catch_all.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace cmf;
namespace fs = std::filesystem;

namespace
{

// Render a JSON line with a given ts_recv second (as 2026-04-07T00:00:ss).
std::string makeLine(int sec_offset, int sequence, int instrument_id,
                     const char* action = "A", const char* side = "B")
{
    std::ostringstream os;
    os << R"({"ts_recv":"2026-04-07T00:00:)" << (sec_offset < 10 ? "0" : "")
       << sec_offset << R"(.000000000Z","hd":{"ts_event":"2026-04-07T00:00:)"
       << (sec_offset < 10 ? "0" : "") << sec_offset
       << R"(.000000000Z","rtype":160,"publisher_id":101,"instrument_id":)"
       << instrument_id << R"(},"action":")" << action << R"(","side":")" << side
       << R"(","price":"1.000000000","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":)"
       << sequence << R"(,"symbol":"TEST"})";
    return os.str();
}

fs::path writeFile(const fs::path& dir, const std::string& name,
                   const std::vector<std::string>& lines)
{
    const fs::path p = dir / name;
    std::ofstream out(p, std::ios::binary);
    REQUIRE(out.is_open());
    for (const auto& l : lines)
        out << l << "\n";
    return p;
}

} // namespace

TEST_CASE("runHardTask - merges 3 files in chronological order", "[HardTask]")
{
    const fs::path dir = fs::temp_directory_path() / "cmf_hardtask_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // Three files, each internally sorted, with interleaved timestamps.
    std::vector<fs::path> files = {
        writeFile(dir, "a.mbo.json",
                  {makeLine(0, 0, 10), makeLine(3, 1, 10), makeLine(6, 2, 10)}),
        writeFile(dir, "b.mbo.json",
                  {makeLine(1, 0, 20), makeLine(4, 1, 20), makeLine(7, 2, 20)}),
        writeFile(dir, "c.mbo.json",
                  {makeLine(2, 0, 30), makeLine(5, 1, 30), makeLine(8, 2, 30)}),
    };

    SECTION("FlatMerger")
    {
        const auto r = runHardTask(files, MergerKind::Flat, /*verbose=*/false);
        REQUIRE(r.total == 9);
        REQUIRE(r.first_ts < r.last_ts);
        REQUIRE(r.elapsed.count() > 0);
    }

    SECTION("HierarchyMerger")
    {
        const auto r = runHardTask(files, MergerKind::Hierarchy, /*verbose=*/false);
        REQUIRE(r.total == 9);
    }

    SECTION("Both strategies produce identical fingerprints")
    {
        const auto a = runHardTask(files, MergerKind::Flat, /*verbose=*/false);
        const auto b = runHardTask(files, MergerKind::Hierarchy, /*verbose=*/false);
        REQUIRE(a.total == b.total);
        REQUIRE(a.first_ts == b.first_ts);
        REQUIRE(a.last_ts == b.last_ts);
        REQUIRE(a.fingerprint == b.fingerprint);

        // Correctness anchor: snapshot taken on the (pre-refactor) simdjson
        // parser. The parser swap (positional byte-reader) must not change the
        // observable output sequence, so this value must survive every refactor
        // of the ingestion hot path. If you deliberately change the fingerprint
        // formula (HardTask.cpp), update this constant in the same commit.
        constexpr std::uint64_t kExpectedFingerprint = 0x3282568c7ef3caaULL;
        REQUIRE(a.fingerprint == kExpectedFingerprint);
    }

    fs::remove_all(dir);
}

TEST_CASE("runHardTask - empty file list returns zero total", "[HardTask]")
{
    const auto r = runHardTask({}, MergerKind::Flat, false);
    REQUIRE(r.total == 0);
}

TEST_CASE("listMboJsonFiles - picks only *.mbo.json files in lexicographic "
          "order",
          "[HardTask]")
{
    const fs::path dir = fs::temp_directory_path() / "cmf_hardtask_listing";
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream(dir / "z.mbo.json");
    }
    {
        std::ofstream(dir / "a.mbo.json");
    }
    {
        std::ofstream(dir / "m.mbo.json");
    }
    {
        std::ofstream(dir / "metadata.json");
    } // must be skipped
    {
        std::ofstream(dir / "other.txt");
    }

    const auto files = listMboJsonFiles(dir);
    REQUIRE(files.size() == 3);
    REQUIRE(files[0].filename() == "a.mbo.json");
    REQUIRE(files[1].filename() == "m.mbo.json");
    REQUIRE(files[2].filename() == "z.mbo.json");

    fs::remove_all(dir);
}
