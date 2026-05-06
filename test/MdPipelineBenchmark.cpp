// benchmarks for market-data pipeline

#include "main/PipelineRunner.hpp"

#include "catch2/catch_all.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
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
        PipelineRunner runner;
        const auto result = runner.run(inputPath);
        const std::size_t earliestHash = result.earliestTimestampNs.has_value()
                                             ? static_cast<std::size_t>(*result.earliestTimestampNs)
                                             : 0;
        const std::size_t latestHash = result.latestTimestampNs.has_value()
                                           ? static_cast<std::size_t>(*result.latestTimestampNs)
                                           : 0;
        return result.totalMessagesProcessed ^ earliestHash ^ latestHash;
    };
}

TEST_CASE("Pipeline Throughput - stream read+parse+summary", "[benchmark][throughput][pipeline]")
{
    const auto inputPath = benchmarkInputPath();
    PipelineRunner runner;

    const auto t0 = std::chrono::steady_clock::now();
    const auto result = runner.run(inputPath);
    const auto t1 = std::chrono::steady_clock::now();

    const double wallSeconds = std::chrono::duration<double>(t1 - t0).count();
    const double throughput = wallSeconds > 0.0 ? static_cast<double>(result.totalMessagesProcessed) / wallSeconds : 0.0;

    std::cout << "\nThroughput summary for " << inputPath << ":\n";
    std::cout << "  Total messages processed: " << result.totalMessagesProcessed << "\n";
    std::cout << "  Wall-clock time (seconds): " << wallSeconds << "\n";
    std::cout << "  Throughput (messages/second): " << throughput << "\n";

    REQUIRE(result.totalMessagesProcessed > 0);
    REQUIRE(result.earliestTimestampNs.has_value());
    REQUIRE(result.latestTimestampNs.has_value());
    REQUIRE(*result.earliestTimestampNs <= *result.latestTimestampNs);
}
