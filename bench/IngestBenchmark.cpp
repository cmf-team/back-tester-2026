#include "common/Auxility.hpp"
#include "common/MarketDataParser.hpp"
#include "ingestion/DataIngestion.hpp"
#include <benchmark/benchmark.h>
#include <filesystem>
#include <fstream>
#include <vector>

std::vector<std::string> GenerateTestFiles(size_t numFiles, size_t numEventsPerFile)
{
    std::vector<std::string> test_files;
    try
    {
        for (size_t f = 0; f < numFiles; ++f)
        {
            std::vector<std::string> ndjson;
            for (size_t i = 0; i < numEventsPerFile; ++i)
            {
                uint64_t ts = 1000 + (f * numEventsPerFile + i) * 500;
                ndjson.push_back(MakeNDJSON(ts, ts, 10000 + f * 1000 + i));
            }
            test_files.push_back(WriteTempNDJSON(ndjson));
        }
        std::sort(test_files.begin(), test_files.end());
    }
    catch (const std::filesystem::filesystem_error&)
    {
        // Test files not available
    }
    return test_files;
}

void CleanupTestFiles(std::vector<std::string>& test_files)
{
    for (const auto& filename : test_files)
        RemoveTempFile(filename);
    test_files.clear();
}

void RegisterBenchmarks()
{
    std::vector<std::pair<int64_t, int64_t>> configs = {
        {1, 20'000}, {1, 40'000}, {1, 80'000}, {4, 30'000}, {10, 10'000}, {13, 17'000}, {16, 20'000}, {20, 50'000}
        // , {20, 500'000}
    };

    for (const auto& [numFiles, numEventsPerFile] : configs)
    {
        benchmark::RegisterBenchmark(
            ("BM_Baseline/" + std::to_string(numFiles) + "files_" + std::to_string(numEventsPerFile) + "events").c_str(),
            [numFiles, numEventsPerFile](benchmark::State& state)
            {
                auto test_files = GenerateTestFiles(numFiles, numEventsPerFile);
                std::size_t event_count = 0;
                uint64_t last_ts = 0;
                bool ordered = true;
                for (auto _ : state)
                {
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
                CleanupTestFiles(test_files);
            });

        benchmark::RegisterBenchmark(
            ("BM_FlatMerger/" + std::to_string(numFiles) + "files_" + std::to_string(numEventsPerFile) + "events").c_str(),
            [numFiles, numEventsPerFile](benchmark::State& state)
            {
                auto test_files = GenerateTestFiles(numFiles, numEventsPerFile);
                std::size_t event_count = 0;
                uint64_t last_ts = 0;
                bool ordered = true;
                FlatMergerEngine engine;
                for (auto _ : state)
                {
                    engine.Ingest(test_files, [&](const MarketDataEvent& e)
                                  {
                        ++event_count;
                        if (e.ts_event < last_ts)
                            ordered = false;
                        last_ts = e.ts_event; });
                    benchmark::DoNotOptimize(event_count);
                    benchmark::DoNotOptimize(ordered);
                }
                CleanupTestFiles(test_files);
            });

        benchmark::RegisterBenchmark(
            ("BM_HierarchyMerger/" + std::to_string(numFiles) + "files_" + std::to_string(numEventsPerFile) + "events").c_str(),
            [numFiles, numEventsPerFile](benchmark::State& state)
            {
                auto test_files = GenerateTestFiles(numFiles, numEventsPerFile);
                std::size_t event_count = 0;
                uint64_t last_ts = 0;
                bool ordered = true;
                HierarchyMergerEngine engine;
                for (auto _ : state)
                {
                    engine.Ingest(test_files, [&](const MarketDataEvent& e)
                                  {
                        ++event_count;
                        if (e.ts_event < last_ts)
                            ordered = false;
                        last_ts = e.ts_event; });
                    benchmark::DoNotOptimize(event_count);
                    benchmark::DoNotOptimize(ordered);
                }
                CleanupTestFiles(test_files);
            });
    }
}

int main(int argc, char* argv[])
{
    RegisterBenchmarks();
    ::benchmark::Initialize(&argc, argv);
    if (::benchmark::ReportUnrecognizedArguments(argc, argv))
        return 1;
    ::benchmark::RunSpecifiedBenchmarks();
    return 0;
}
