#include "common/MarketDataParser.hpp"
#include "ingestion/DataIngestion.hpp"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <fstream>
#include <vector>

// Fixture for benchmark setup using actual test data
class IngestBenchmarkFixture : public benchmark::Fixture
{
  protected:
    std::vector<std::string> test_files;

    void SetUp(const benchmark::State& state) override
    {
        (void)state;
        // Load actual test files from test/data/T6R_407/1000_counts/
        std::string test_dir = "test/data/T6R_407/1000_counts";
        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(test_dir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".ndjson")
                {
                    test_files.push_back(entry.path().string());
                }
            }
            std::sort(test_files.begin(), test_files.end());
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // Test files not available
        }
    }

    void TearDown(const benchmark::State& state) override
    {
        (void)state;
        test_files.clear();
    }
};

// Baseline: single-threaded sequential parsing (no merger, no threading)
BENCHMARK_F(IngestBenchmarkFixture, BM_Baseline)(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::size_t event_count = 0;
        uint64_t last_ts = 0;
        bool ordered = true;
        for (const auto& filename : test_files)
        {
            std::ifstream file{filename};
            std::string line;
            while (std::getline(file, line))
            {
                auto event = parseNDJSON(line);
                if (event)
                {
                    ++event_count;
                    if (event->ts_event < last_ts)
                        ordered = false;
                    last_ts = event->ts_event;
                }
            }
        }
        benchmark::DoNotOptimize(event_count);
        benchmark::DoNotOptimize(ordered);
    }
}

// FlatMerger: parallel parsing with flat merger
BENCHMARK_F(IngestBenchmarkFixture, BM_FlatMerger)(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::size_t event_count = 0;
        uint64_t last_ts = 0;
        bool ordered = true;
        FlatMergerEngine engine;
        engine.Ingest(test_files, [&](const MarketDataEvent& e)
                      {
            ++event_count;
            if (e.ts_event < last_ts)
                ordered = false;
            last_ts = e.ts_event; });
        benchmark::DoNotOptimize(event_count);
        benchmark::DoNotOptimize(ordered);
    }
}

// HierarchyMerger: parallel parsing with hierarchy merger
BENCHMARK_F(IngestBenchmarkFixture, BM_HierarchyMerger)(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::size_t event_count = 0;
        uint64_t last_ts = 0;
        bool ordered = true;
        HierarchyMergerEngine engine;
        engine.Ingest(test_files, [&](const MarketDataEvent& e)
                      {
            ++event_count;
            if (e.ts_event < last_ts)
                ordered = false;
            last_ts = e.ts_event; });
        benchmark::DoNotOptimize(event_count);
        benchmark::DoNotOptimize(ordered);
    }
}

BENCHMARK_MAIN();
