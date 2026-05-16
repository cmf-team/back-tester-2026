#include "runners/HardRunnerSupport.hpp"

#include "domain/MarketDataEvent.hpp"
#include "io/MmapFile.hpp"
#include "parsing/JsonParser.hpp"
#ifdef MD_ENABLE_ARROW
#include "runners/FeatherHardRunnerSupport.hpp"
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>

namespace md {
namespace {

struct HeapNode {
    MarketDataEvent event;
    std::size_t input_index{};
};

struct HeapCompare {
    bool operator()(const HeapNode& lhs, const HeapNode& rhs) const {
        // std::priority_queue returns the "largest" element first, so reverse the chronological order.
        return eventComesBefore(rhs.event, lhs.event);
    }
};

} // namespace

EventQueuePtr makeEventQueue() {
    return std::make_shared<EventQueue>(event_queue_batch_size);
}

void ProducerSet::join() {
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

ParseDiagnostics ProducerSet::combinedDiagnostics() const {
    ParseDiagnostics combined;
    for (const auto& item : diagnostics) {
        combined.add(item);
    }
    return combined;
}

void validateReadableFiles(const std::vector<std::filesystem::path>& files) {
    for (const auto& file_path : files) {
        if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
            throw std::runtime_error("input file is not readable: " + file_path.string());
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw std::runtime_error("cannot open input file: " + file_path.string());
        }
    }
}

ProducerSet startProducerThreads(
    const std::vector<std::filesystem::path>& files,
    bool verbose,
    std::ostream& err,
    InputFormat input_format
) {
    if (input_format == InputFormat::Feather) {
#ifdef MD_ENABLE_ARROW
        return startFeatherProducerThreads(files, verbose, err);
#else
        throw std::runtime_error(
            "Feather input requires an Arrow-enabled build. "
            "Rebuild with -DENABLE_ARROW=ON."
        );
#endif
    }

    validateReadableFiles(files);

    if (verbose) {
        err << "producer_threads=" << files.size() << '\n'
            << "reader=mmap\n";
    }

    ProducerSet producers;
    producers.queues.reserve(files.size());
    producers.threads.reserve(files.size());
    producers.diagnostics.resize(files.size());

    for (std::size_t index = 0; index < files.size(); ++index) {
        producers.queues.push_back(makeEventQueue());
    }

    for (std::size_t index = 0; index < files.size(); ++index) {
        EventQueuePtr queue = producers.queues[index];
        ParseDiagnostics* diagnostics = &producers.diagnostics[index];
        const std::filesystem::path file_path = files[index];

        producers.threads.emplace_back(
            [index, file_path, queue, diagnostics] {
                MmapFile file{file_path};
                std::size_t line_number = 0;
                while (auto line = file.nextLine()) {
                    ++line_number;
                    ++diagnostics->total_lines_read;

                    queue->push(QueueItem::data(parseMarketDataEventLine(
                        *line,
                        line_number,
                        static_cast<std::uint32_t>(index),
                        static_cast<std::uint64_t>(line_number)
                    )));
                }

                queue->push(QueueItem::end());
                queue->flush();
            }
        );
    }

    return producers;
}

void mergeInputQueues(const std::vector<EventQueuePtr>& inputs, const EventQueuePtr& output) {
    if (inputs.empty()) {
        output->push(QueueItem::end());
        output->flush();
        return;
    }

    std::priority_queue<HeapNode, std::vector<HeapNode>, HeapCompare> heap;

    for (std::size_t input_index = 0; input_index < inputs.size(); ++input_index) {
        QueueItem item = inputs[input_index]->pop();
        if (!item.end_of_stream) {
            heap.push(HeapNode{item.event, input_index});
        }
    }

    while (!heap.empty()) {
        HeapNode next = heap.top();
        heap.pop();

        output->push(QueueItem::data(next.event));

        QueueItem replacement = inputs[next.input_index]->pop();
        if (!replacement.end_of_stream) {
            heap.push(HeapNode{replacement.event, next.input_index});
        }
    }

    output->push(QueueItem::end());
    output->flush();
}

void dispatchMergedQueue(
    const EventQueuePtr& input,
    IMarketDataEventProcessor& processor,
    ProcessingSummary& summary
) {
    while (true) {
        QueueItem item = input->pop();
        if (item.end_of_stream) {
            return;
        }

        processor.processMarketDataEvent(item.event);
        summary.observe(item.event);
    }
}

} // namespace md
