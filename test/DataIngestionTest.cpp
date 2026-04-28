#include "ingestion/DataIngestion.hpp"
#include "common/MarketDataParser.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

// Helper to generate a minimal valid NDJSON line
static auto MakeNDJSON(uint64_t ts_event_ns, uint64_t ts_recv_ns, uint64_t order_id) -> std::string
{
    auto ts_event_iso = nanosToIso(ts_event_ns);
    auto ts_recv_iso = nanosToIso(ts_recv_ns);

    std::ostringstream oss;
    oss << R"({"ts_recv":")" << ts_recv_iso << R"(","hd":{"ts_event":")" << ts_event_iso
        << R"(","rtype":160,"publisher_id":1,"instrument_id":100},"action":"A","side":"B","price":"10000000000","size":10,"channel_id":0,"order_id":")"
        << order_id << R"(","flags":0,"ts_in_delta":100})";
    return oss.str();
}

// Helper to write NDJSON lines to a temp file
static auto WriteTempNDJSON(const std::vector<std::string>& lines) -> std::string
{
    std::string filename = std::tmpnam(nullptr);
    std::ofstream file{filename};
    for (const auto& line : lines)
    {
        file << line << "\n";
    }
    file.close();
    return filename;
}

// Helper to clean up temp file
static void RemoveTempFile(const std::string& filename)
{
    std::filesystem::remove(filename);
}

TEST_CASE("DataIngestion - FlatMerger single file", "[DataIngestion][FlatMerger]")
{
    std::vector<std::string> ndjson;
    for (int i = 0; i < 5; ++i)
    {
        ndjson.push_back(MakeNDJSON(1000 + i * 100, 1000 + i * 100, 1000 + i));
    }

    std::string filename = WriteTempNDJSON(ndjson);

    std::vector<MarketDataEvent> events;
    FlatMergerEngine engine;
    engine.Ingest({filename}, [&](const MarketDataEvent& e)
                  { events.push_back(e); });

    REQUIRE(events.size() == 5);
    for (std::size_t i = 0; i < events.size() - 1; ++i)
    {
        REQUIRE(events[i].ts_event <= events[i + 1].ts_event);
    }

    RemoveTempFile(filename);
}

TEST_CASE("DataIngestion - FlatMerger two interleaved files", "[DataIngestion][FlatMerger]")
{
    // File 1: timestamps 1000, 3000, 5000, 7000, 9000
    std::vector<std::string> file1_ndjson;
    for (int i = 0; i < 5; ++i)
    {
        file1_ndjson.push_back(MakeNDJSON(1000 + i * 2000, 1000 + i * 2000, 10000 + i));
    }

    // File 2: timestamps 2000, 4000, 6000, 8000, 10000
    std::vector<std::string> file2_ndjson;
    for (int i = 0; i < 5; ++i)
    {
        file2_ndjson.push_back(MakeNDJSON(2000 + i * 2000, 2000 + i * 2000, 20000 + i));
    }

    std::string file1 = WriteTempNDJSON(file1_ndjson);
    std::string file2 = WriteTempNDJSON(file2_ndjson);

    std::vector<MarketDataEvent> events;
    FlatMergerEngine engine;
    engine.Ingest({file1, file2}, [&](const MarketDataEvent& e)
                  { events.push_back(e); });

    REQUIRE(events.size() == 10);
    for (std::size_t i = 0; i < events.size() - 1; ++i)
    {
        REQUIRE(events[i].ts_event <= events[i + 1].ts_event);
    }

    RemoveTempFile(file1);
    RemoveTempFile(file2);
}

TEST_CASE("DataIngestion - FlatMerger empty file", "[DataIngestion][FlatMerger]")
{
    std::vector<std::string> file1_ndjson;
    std::vector<std::string> file2_ndjson{
        MakeNDJSON(1000, 1000, 1000),
        MakeNDJSON(2000, 2000, 1001),
        MakeNDJSON(3000, 3000, 1002)};

    std::string file1 = WriteTempNDJSON(file1_ndjson);
    std::string file2 = WriteTempNDJSON(file2_ndjson);

    std::vector<MarketDataEvent> events;
    FlatMergerEngine engine;
    engine.Ingest({file1, file2}, [&](const MarketDataEvent& e)
                  { events.push_back(e); });

    REQUIRE(events.size() == 3);

    RemoveTempFile(file1);
    RemoveTempFile(file2);
}

TEST_CASE("DataIngestion - HierarchyMerger four files", "[DataIngestion][HierarchyMerger]")
{
    // Create 4 files with interleaved timestamps
    std::vector<std::string> files;
    for (int f = 0; f < 4; ++f)
    {
        std::vector<std::string> ndjson;
        for (int i = 0; i < 5; ++i)
        {
            uint64_t ts = 1000 + (f * 5 + i) * 500;
            ndjson.push_back(MakeNDJSON(ts, ts, 10000 + f * 1000 + i));
        }
        files.push_back(WriteTempNDJSON(ndjson));
    }

    std::vector<MarketDataEvent> events;
    HierarchyMergerEngine engine;
    engine.Ingest(files, [&](const MarketDataEvent& e)
                  { events.push_back(e); });

    REQUIRE(events.size() == 20);
    for (std::size_t i = 0; i < events.size() - 1; ++i)
    {
        REQUIRE(events[i].ts_event <= events[i + 1].ts_event);
    }

    for (const auto& file : files)
    {
        RemoveTempFile(file);
    }
}

TEST_CASE("DataIngestion - HierarchyMerger three files (odd)", "[DataIngestion][HierarchyMerger]")
{
    // Create 3 files (odd number) with interleaved timestamps
    std::vector<std::string> files;
    for (int f = 0; f < 3; ++f)
    {
        std::vector<std::string> ndjson;
        for (int i = 0; i < 4; ++i)
        {
            uint64_t ts = 1000 + (f * 4 + i) * 333;
            ndjson.push_back(MakeNDJSON(ts, ts, 20000 + f * 1000 + i));
        }
        files.push_back(WriteTempNDJSON(ndjson));
    }

    std::vector<MarketDataEvent> events;
    HierarchyMergerEngine engine;
    engine.Ingest(files, [&](const MarketDataEvent& e)
                  { events.push_back(e); });

    REQUIRE(events.size() == 12);
    for (std::size_t i = 0; i < events.size() - 1; ++i)
    {
        REQUIRE(events[i].ts_event <= events[i + 1].ts_event);
    }

    for (const auto& file : files)
    {
        RemoveTempFile(file);
    }
}

TEST_CASE("DataIngestion - Both strategies event count", "[DataIngestion]")
{
    // Create 2 files with known event counts
    std::vector<std::string> file1_ndjson{
        MakeNDJSON(1000, 1000, 1000),
        MakeNDJSON(2000, 2000, 1001),
        MakeNDJSON(3000, 3000, 1002)};
    std::vector<std::string> file2_ndjson{
        MakeNDJSON(1500, 1500, 2000),
        MakeNDJSON(2500, 2500, 2001),
        MakeNDJSON(3500, 3500, 2002),
        MakeNDJSON(4000, 4000, 2003)};

    std::string file1 = WriteTempNDJSON(file1_ndjson);
    std::string file2 = WriteTempNDJSON(file2_ndjson);

    // Test FlatMerger
    std::vector<MarketDataEvent> flat_events;
    FlatMergerEngine flat_engine;
    flat_engine.Ingest({file1, file2}, [&](const MarketDataEvent& e)
                       { flat_events.push_back(e); });
    REQUIRE(flat_events.size() == 7);

    // Test HierarchyMerger
    std::vector<MarketDataEvent> hier_events;
    HierarchyMergerEngine hier_engine;
    hier_engine.Ingest({file1, file2}, [&](const MarketDataEvent& e)
                       { hier_events.push_back(e); });
    REQUIRE(hier_events.size() == 7);

    RemoveTempFile(file1);
    RemoveTempFile(file2);
}
