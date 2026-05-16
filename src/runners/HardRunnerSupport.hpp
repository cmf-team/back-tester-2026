#pragma once

#include "concurrency/NonBlockingQueue.hpp"
#include "processing/IMarketDataEventProcessor.hpp"
#include "runners/InputFormat.hpp"
#include "runners/QueueItem.hpp"
#include "runners/RunResult.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <thread>
#include <vector>

namespace md {

using EventQueue = NonBlockingQueue<QueueItem>;
using EventQueuePtr = std::shared_ptr<EventQueue>;

inline constexpr std::size_t event_queue_batch_size = 256;

EventQueuePtr makeEventQueue();

struct ProducerSet {
    std::vector<EventQueuePtr> queues;
    std::vector<std::thread> threads;
    std::vector<ParseDiagnostics> diagnostics;

    void join();
    [[nodiscard]] ParseDiagnostics combinedDiagnostics() const;
};

ProducerSet startProducerThreads(
    const std::vector<std::filesystem::path>& files,
    bool verbose,
    std::ostream& err,
    InputFormat input_format = InputFormat::Json
);

void mergeInputQueues(const std::vector<EventQueuePtr>& inputs, const EventQueuePtr& output);

void dispatchMergedQueue(
    const EventQueuePtr& input,
    IMarketDataEventProcessor& processor,
    ProcessingSummary& summary
);

void validateReadableFiles(const std::vector<std::filesystem::path>& files);

} // namespace md
