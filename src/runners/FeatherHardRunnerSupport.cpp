#include "runners/FeatherHardRunnerSupport.hpp"

#include "io/FeatherEventReader.hpp"

#include <ostream>

namespace md {

ProducerSet startFeatherProducerThreads(
    const std::vector<std::filesystem::path>& files,
    bool verbose,
    std::ostream& err
) {
    validateReadableFiles(files);

    if (verbose) {
        err << "producer_threads=" << files.size() << '\n'
            << "reader=feather\n";
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
                FeatherEventReader reader{file_path};
                reader.readAll(
                    static_cast<std::uint32_t>(index),
                    [queue, diagnostics](const MarketDataEvent& event) {
                        ++diagnostics->total_lines_read;
                        queue->push(QueueItem::data(event));
                    }
                );

                queue->push(QueueItem::end());
                queue->flush();
            }
        );
    }

    return producers;
}

} // namespace md
