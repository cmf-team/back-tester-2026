#include "runners/BenchmarkRunner.hpp"

#include "processing/LoggingMarketDataEventProcessor.hpp"
#include "processing/LobMarketDataEventProcessor.hpp"
#include "processing/ShardedLobMarketDataEventProcessor.hpp"
#include "runners/FlatMergeRunner.hpp"
#include "runners/HierarchicalMergeRunner.hpp"

#include <sstream>
#include <string>
#include <utility>

namespace md {
namespace {

LobProcessorConfig benchmarkLobConfig() {
    LobProcessorConfig config;
    config.snapshot_depth = 0;
    config.snapshot_interval_events = 0;
    config.max_snapshots = 0;
    return config;
}

BenchmarkResult makeLobBenchmarkResult(
    RunResult result,
    const LobMarketDataEventProcessor& processor,
    InputFormat input_format,
    std::size_t lob_workers
) {
    BenchmarkResult benchmark;
    benchmark.result = std::move(result);
    benchmark.input_format = std::string(inputFormatName(input_format));
    benchmark.processor = "lob";
    benchmark.unresolved_events = processor.books().unresolvedEvents();
    benchmark.lob_workers = lob_workers;
    benchmark.lob_digest = processor.books().stableStateDigest();
    return benchmark;
}

BenchmarkResult makeShardedLobBenchmarkResult(
    RunResult result,
    ShardedLobMarketDataEventProcessor& processor,
    InputFormat input_format,
    std::size_t lob_workers
) {
    processor.finish();

    BenchmarkResult benchmark;
    benchmark.result = std::move(result);
    benchmark.input_format = std::string(inputFormatName(input_format));
    benchmark.processor = "lob-sharded-" + std::to_string(lob_workers);
    benchmark.unresolved_events = processor.unresolvedEvents();
    benchmark.lob_workers = lob_workers;
    benchmark.lob_digest = processor.stableStateDigest();
    return benchmark;
}

BenchmarkResult makeLoggingBenchmarkResult(RunResult result, InputFormat input_format) {
    BenchmarkResult benchmark;
    benchmark.result = std::move(result);
    benchmark.input_format = std::string(inputFormatName(input_format));
    benchmark.processor = "logging";
    benchmark.unresolved_events = 0;
    return benchmark;
}

} // namespace

std::vector<BenchmarkResult> runLoggingBenchmark(
    const std::filesystem::path& folder_path,
    InputFormat input_format,
    bool verbose,
    std::ostream& err
) {
    std::vector<BenchmarkResult> results;
    results.reserve(2);

    {
        std::ostringstream sink;
        LoggingMarketDataEventProcessor processor(sink, 0);
        results.push_back(makeLoggingBenchmarkResult(
            FlatMergeRunner{}.run(folder_path, processor, verbose, err, input_format),
            input_format
        ));
    }

    {
        std::ostringstream sink;
        LoggingMarketDataEventProcessor processor(sink, 0);
        results.push_back(makeLoggingBenchmarkResult(
            HierarchicalMergeRunner{}.run(folder_path, processor, verbose, err, input_format),
            input_format
        ));
    }

    return results;
}

std::vector<BenchmarkResult> runLobBenchmark(
    const std::filesystem::path& folder_path,
    InputFormat input_format,
    bool verbose,
    std::ostream& err,
    std::size_t lob_workers
) {
    std::vector<BenchmarkResult> results;
    results.reserve(2);

    {
        std::ostringstream sink;
        if (lob_workers > 1) {
            ShardedLobMarketDataEventProcessor processor(sink, lob_workers, benchmarkLobConfig());
            auto result = FlatMergeRunner{}.run(folder_path, processor, verbose, err, input_format);
            results.push_back(makeShardedLobBenchmarkResult(
                std::move(result),
                processor,
                input_format,
                lob_workers
            ));
        } else {
            LobMarketDataEventProcessor processor(sink, benchmarkLobConfig());
            auto result = FlatMergeRunner{}.run(folder_path, processor, verbose, err, input_format);
            results.push_back(makeLobBenchmarkResult(
                std::move(result),
                processor,
                input_format,
                lob_workers
            ));
        }
    }

    {
        std::ostringstream sink;
        if (lob_workers > 1) {
            ShardedLobMarketDataEventProcessor processor(sink, lob_workers, benchmarkLobConfig());
            auto result = HierarchicalMergeRunner{}.run(folder_path, processor, verbose, err, input_format);
            results.push_back(makeShardedLobBenchmarkResult(
                std::move(result),
                processor,
                input_format,
                lob_workers
            ));
        } else {
            LobMarketDataEventProcessor processor(sink, benchmarkLobConfig());
            auto result = HierarchicalMergeRunner{}.run(folder_path, processor, verbose, err, input_format);
            results.push_back(makeLobBenchmarkResult(
                std::move(result),
                processor,
                input_format,
                lob_workers
            ));
        }
    }

    return results;
}

} // namespace md
