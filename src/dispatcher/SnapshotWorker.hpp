// SnapshotWorker — async stateless I/O for periodic LOB snapshots.
//
// The dispatcher thread builds a SnapshotTask (cheap O(1) Bbo per touched
// instrument) and pushes it onto an SPSC queue. The worker thread drains
// the queue and writes to the configured output stream. This keeps the
// dispatcher's hot loop free of formatting/IO costs while preserving the
// chronological invariant — snapshots are tagged with the dispatcher event
// index so they remain in order in the output.

#pragma once

#include "market_data/SpscRingQueue.hpp"
#include "order_book/LimitOrderBook.hpp"

#include <cstdint>
#include <iosfwd>
#include <thread>
#include <vector>

namespace cmf
{

struct SnapshotEntry
{
    std::uint64_t instrument_id{0};
    LimitOrderBook::Bbo bbo;
};

struct SnapshotTask
{
    std::uint64_t event_index{0};
    std::vector<SnapshotEntry> entries;
};

class SnapshotWorker
{
  public:
    // `out == nullptr` disables snapshot printing (useful for benchmark mode).
    // Otherwise spawns a worker thread that drains the queue.
    explicit SnapshotWorker(std::ostream* out, std::size_t queue_capacity = 256);

    SnapshotWorker(const SnapshotWorker&) = delete;
    SnapshotWorker& operator=(const SnapshotWorker&) = delete;

    ~SnapshotWorker();

    // Enqueue a task. No-op if printing is disabled. Blocks (yields) until
    // the queue has slack — backpressure protects against runaway memory.
    void enqueue(SnapshotTask task);

    // Closes the queue and joins the worker. Idempotent.
    void stop();

  private:
    void run();

    std::ostream* out_;
    SpscRingQueue<SnapshotTask> queue_;
    std::thread thread_;
    bool stopped_{false};
};

} // namespace cmf
