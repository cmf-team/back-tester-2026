// benchmarks for market-data pipeline

#include "common/MarketDataEvent.hpp"
#include "main/MdEventConverter.hpp"
#include "main/FileReader.hpp"

#include "catch2/catch_all.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace cmf;

namespace
{

std::string benchmarkInputPath()
{
    if (const char *path = std::getenv("MBO_BENCH_FILE"); path != nullptr && path[0] != '\0')
    {
        return std::string(path);
    }
    return std::string(BACK_TESTER_SOURCE_DIR) + "/test/data/ndjson-sample.json";
}

} // namespace

TEST_CASE("Market Data Pipeline Benchmark - stream read+parse+summary", "[benchmark][pipeline]")
{
    const auto inputPath = benchmarkInputPath();

    BENCHMARK("full flow stream read+parse+summary")
    {
        MdEventConverter converter;
        FileReader reader(inputPath);
        MarketDataEvent event;
        std::string rawLine;
        std::size_t totalProcessed = 0;
        NanoTime earliestTs = std::numeric_limits<NanoTime>::max();
        NanoTime latestTs = std::numeric_limits<NanoTime>::min();

        while (reader.readNextRawLine(rawLine))
        {
            if (!converter.parseRaw(rawLine, event))
            {
                continue;
            }
            ++totalProcessed;
            if (event.tsRecv < earliestTs)
            {
                earliestTs = event.tsRecv;
            }
            if (event.tsRecv > latestTs)
            {
                latestTs = event.tsRecv;
            }
        }

        return totalProcessed ^ static_cast<std::size_t>(earliestTs) ^ static_cast<std::size_t>(latestTs);
    };
}

TEST_CASE("Pipeline Throughput - stream read+parse+summary", "[benchmark][throughput][pipeline]")
{
    const auto inputPath = benchmarkInputPath();
    MdEventConverter converter;
    FileReader reader(inputPath);
    MarketDataEvent event;
    std::string rawLine;
    std::size_t totalProcessed = 0;
    NanoTime earliestTs = std::numeric_limits<NanoTime>::max();
    NanoTime latestTs = std::numeric_limits<NanoTime>::min();

    const auto t0 = std::chrono::steady_clock::now();
    while (reader.readNextRawLine(rawLine))
    {
        if (!converter.parseRaw(rawLine, event))
        {
            continue;
        }
        ++totalProcessed;
        if (event.tsRecv < earliestTs)
        {
            earliestTs = event.tsRecv;
        }
        if (event.tsRecv > latestTs)
        {
            latestTs = event.tsRecv;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();

    const double wallSeconds = std::chrono::duration<double>(t1 - t0).count();
    const double throughput = wallSeconds > 0.0 ? static_cast<double>(totalProcessed) / wallSeconds : 0.0;

    std::cout << "\nThroughput summary for " << inputPath << ":\n";
    std::cout << "  Total messages processed: " << totalProcessed << "\n";
    std::cout << "  Wall-clock time (seconds): " << wallSeconds << "\n";
    std::cout << "  Throughput (messages/second): " << throughput << "\n";

    REQUIRE(totalProcessed > 0);
    REQUIRE(earliestTs <= latestTs);
}
