#include "runners/HierarchicalMergeRunner.hpp"

#include "runners/HardRunnerSupport.hpp"
#include "runners/InputFileDiscovery.hpp"

#include <algorithm>
#include <chrono>
#include <ostream>
#include <thread>
#include <utility>
#include <vector>

namespace md {
namespace {

constexpr std::size_t merge_fan_in = 4; // 4-way tree: fewer intermediate queue hops while preserving deterministic ordering.

std::vector<std::thread> buildMergeTree(
    std::vector<EventQueuePtr> current_level,
    EventQueuePtr& final_queue
) {
    std::vector<std::thread> merger_threads;

    if (current_level.empty()) {
        final_queue = makeEventQueue();
        final_queue->push(QueueItem::end());
        final_queue->flush();
        return merger_threads;
    }

    while (current_level.size() > 1) {
        std::vector<EventQueuePtr> next_level;
        next_level.reserve((current_level.size() + merge_fan_in - 1) / merge_fan_in);

        for (std::size_t i = 0; i < current_level.size(); i += merge_fan_in) {
            const std::size_t end = std::min(i + merge_fan_in, current_level.size());

            if (end - i == 1) {
                next_level.push_back(current_level[i]);
                continue;
            }

            std::vector<EventQueuePtr> group;
            group.reserve(end - i);
            for (std::size_t j = i; j < end; ++j) {
                group.push_back(current_level[j]);
            }

            auto output = makeEventQueue();
            merger_threads.emplace_back([group = std::move(group), output] {
                mergeInputQueues(group, output);
            });
            next_level.push_back(output);
        }

        current_level = std::move(next_level);
    }

    final_queue = current_level.front();
    return merger_threads;
}

} // namespace

RunResult HierarchicalMergeRunner::run(
    const std::filesystem::path& folder_path,
    IMarketDataEventProcessor& processor,
    bool verbose,
    std::ostream& err,
    InputFormat input_format
) const {
    RunResult result;
    result.strategy_name = "hierarchy";

    const auto files = discoverInputFiles(folder_path, input_format);
    if (verbose) {
        err << "selected_mode=hierarchy\n"
            << "input_format=" << inputFormatName(input_format) << '\n'
            << "discovered_files_count=" << files.size() << '\n'
            << "merge_strategy=4_way_tree\n";
    }

    const auto started_at = std::chrono::steady_clock::now();

    ProducerSet producers = startProducerThreads(
        files,
        verbose,
        err,
        input_format
    );

    EventQueuePtr final_queue;
    std::vector<std::thread> merger_threads = buildMergeTree(producers.queues, final_queue);

    std::thread dispatcher([&] {
        dispatchMergedQueue(final_queue, processor, result.summary);
    });

    producers.join();

    for (auto& thread : merger_threads) {
        if (thread.joinable()) {
            thread.join();
        }
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
