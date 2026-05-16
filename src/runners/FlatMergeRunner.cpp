#include "runners/FlatMergeRunner.hpp"

#include "runners/HardRunnerSupport.hpp"
#include "runners/InputFileDiscovery.hpp"

#include <chrono>
#include <ostream>
#include <thread>

namespace md {

RunResult FlatMergeRunner::run(
    const std::filesystem::path& folder_path,
    IMarketDataEventProcessor& processor,
    bool verbose,
    std::ostream& err,
    InputFormat input_format
) const {
    RunResult result;
    result.strategy_name = "flat";

    const auto files = discoverInputFiles(folder_path, input_format);
    if (verbose) {
        err << "selected_mode=flat\n"
            << "input_format=" << inputFormatName(input_format) << '\n'
            << "discovered_files_count=" << files.size() << '\n'
            << "merge_strategy=single_level_k_way_heap\n";
    }

    const auto started_at = std::chrono::steady_clock::now();

    ProducerSet producers = startProducerThreads(
        files,
        verbose,
        err,
        input_format
    );
    auto merged_queue = makeEventQueue();

    std::thread merger([&producers, merged_queue] {
        mergeInputQueues(producers.queues, merged_queue);
    });

    std::thread dispatcher([&] {
        dispatchMergedQueue(merged_queue, processor, result.summary);
    });

    producers.join();
    if (merger.joinable()) {
        merger.join();
    }
    if (dispatcher.joinable()) {
        dispatcher.join();
    }

    result.diagnostics = producers.combinedDiagnostics();
    const auto finished_at = std::chrono::steady_clock::now();
    result.wall_clock_seconds = std::chrono::duration<double>(finished_at - started_at).count();

    if (verbose) {
        err << "messages_processed=" << result.summary.total_messages_processed << '\n'
            << "chronological_violations=" << result.summary.chronological_violations << '\n';
    }

    return result;
}

} // namespace md
