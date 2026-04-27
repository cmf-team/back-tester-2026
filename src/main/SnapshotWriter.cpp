#include "SnapshotWriter.hpp"

namespace cmf {

SnapshotWriter::SnapshotWriter(const std::string& logPath)
    : out_(logPath),
      thread_(&SnapshotWriter::run, this)
{
    out_ << "ts,event_count,instrument_id,best_bid,bid_size,best_ask,ask_size\n";
}

SnapshotWriter::~SnapshotWriter() {
    close();
}

void SnapshotWriter::submit(const Snapshot& snap) {
    queue_.push(snap);
}

void SnapshotWriter::close() {
    if (closed_) return;
    closed_ = true;
    queue_.setDone();
    if (thread_.joinable()) thread_.join();
}

void SnapshotWriter::run() {
    Snapshot s;
    while (queue_.pop(s)) {
        out_ << s.ts << ','
             << s.eventCount << ','
             << s.instrument_id << ','
             << s.bestBid << ','
             << s.bidSize << ','
             << s.bestAsk << ','
             << s.askSize << '\n';
    }
}

} // namespace cmf
