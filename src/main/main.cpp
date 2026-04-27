// main function for the back-tester app
// please, keep it minimalistic
#include "Dispatcher.hpp"
#include "EventParser.hpp"
#include "LineReader.hpp"
#include "MarketDataEvent.hpp"
#include "Merger.hpp"
#include "SnapshotWriter.hpp"
#include "ThreadSafeQueue.hpp"
#include "common/BasicTypes.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;
using namespace cmf;

namespace {

constexpr size_t SNAPSHOT_EVERY_N_EVENTS = 1'000'000;

void produce(const std::string& filePath, ThreadSafeQueue<MarketDataEvent>& queue) {
    LineReader reader(filePath);
    std::string line;
    while (reader.nextLine(line)) {
        try {
            queue.push(parseEvent(line));
        } catch (const std::exception&) {
            // skip malformed lines
        }
    }
    queue.setDone();
}

std::vector<std::string> collectFiles(const std::string& dirPath) {
    std::vector<std::string> files;
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        std::string filename = entry.path().filename().string();
        if (filename.size() > 9 &&
            filename.substr(filename.size() - 9) == ".mbo.json") {
            files.push_back(entry.path().string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Periodic snapshot of the most-active book → push to snapshot writer.
// This is "stateless work" relative to the dispatcher: we read book state,
// pack a struct, and the actual file I/O happens on a different thread.
void maybeSnapshot(const Dispatcher& dispatcher,
                   SnapshotWriter& writer,
                   const MarketDataEvent& event) {
    if (dispatcher.eventsProcessed() % SNAPSHOT_EVERY_N_EVENTS != 0) return;

    uint32_t inst = event.instrument_id;
    const LimitOrderBook* lob = dispatcher.book(inst);
    if (lob == nullptr) return;

    Snapshot snap{
        event.ts_recv,
        dispatcher.eventsProcessed(),
        inst,
        lob->bestBid(),
        lob->bestAsk(),
        lob->bestBidSize(),
        lob->bestAskSize()
    };
    writer.submit(snap);
}

void runBenchmark(
    const std::string& name,
    const std::vector<std::string>& files,
    std::function<size_t(std::vector<ThreadSafeQueue<MarketDataEvent>>&,
                         std::function<void(const MarketDataEvent&)>)> merger
) {
    size_t n = files.size();
    std::vector<ThreadSafeQueue<MarketDataEvent>> queues(n);
    std::vector<std::thread> producerThreads;

    Dispatcher dispatcher;
    SnapshotWriter writer("snapshots_" + name + ".csv");

    // The dispatcher consumes events; the writer thread runs in parallel
    // for stateless I/O. LOB updates themselves stay sequential.
    auto consumer = [&dispatcher, &writer](const MarketDataEvent& event) {
        dispatcher.process(event);
        maybeSnapshot(dispatcher, writer, event);
    };

    auto start = std::chrono::steady_clock::now();

    for (size_t i = 0; i < n; ++i) {
        producerThreads.emplace_back(produce, files[i], std::ref(queues[i]));
    }

    size_t count = merger(queues, consumer);

    for (auto& t : producerThreads) t.join();
    writer.close();

    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    // === report ===
    std::cout << "=== " << name << " ===\n";
    std::cout << "Total messages: " << count << '\n';
    std::cout << "Events processed (dispatcher): " << dispatcher.eventsProcessed() << '\n';
    std::cout << "Distinct instruments: " << dispatcher.bookCount() << '\n';
    std::cout << "Resident orders at end: " << dispatcher.orderCount() << '\n';
    std::cout << "Wall-clock time: " << elapsed << " sec\n";
    std::cout << "Throughput: " << static_cast<size_t>(count / elapsed) << " msg/sec\n";

    // final book of the most-active instrument
    uint32_t topInst = dispatcher.mostActiveInstrument();
    const LimitOrderBook* lob = dispatcher.book(topInst);
    if (lob != nullptr && !lob->empty()) {
        std::cout << "\nFinal book of most-active instrument " << topInst << ":\n";
        lob->printSnapshot(std::cout, 5);
        std::cout << "Best bid: " << lob->bestBid() << " x " << lob->bestBidSize()
                  << "  |  Best ask: " << lob->bestAsk() << " x " << lob->bestAskSize() << '\n';
    }
    std::cout << '\n';
}

} // anonymous namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] const char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: back-tester <path-to-folder>\n";
        return 1;
    }

    std::string dirPath = argv[1];
    std::vector<std::string> files = collectFiles(dirPath);

    std::cout << "Found " << files.size() << " files\n\n";

    runBenchmark("flat",      files, flatMerge);
    runBenchmark("hierarchy", files, hierarchyMerge);

    return 0;
}
