#pragma once

#include "ThreadSafeQueue.hpp"
#include "common/BasicTypes.hpp"

#include <fstream>
#include <string>
#include <thread>

namespace cmf {

// lightweight snapshot of one book at one moment
struct Snapshot {
    NanoTime ts            = 0;
    size_t   eventCount    = 0;
    uint32_t instrument_id = 0;
    Price    bestBid       = 0.0;
    Price    bestAsk       = 0.0;
    Quantity bidSize       = 0.0;
    Quantity askSize       = 0.0;
};

// Producer-consumer pattern, just like LineReader→merger:
// dispatcher is the producer (calls submit), this writer is the consumer
// (its own thread pops from the queue and writes to file).
// Keeps slow file I/O off the critical event-processing path.
class SnapshotWriter {
public:
    explicit SnapshotWriter(const std::string& logPath);
    ~SnapshotWriter();

    void submit(const Snapshot& snap);
    void close();

private:
    void run();

    ThreadSafeQueue<Snapshot> queue_;
    std::ofstream             out_;
    std::thread               thread_;
    bool                      closed_ = false;
};

} // namespace cmf
