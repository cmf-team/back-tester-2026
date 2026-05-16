#include "TestSupport.hpp"

#include "processing/AsyncSnapshotWriter.hpp"

#include <sstream>
#include <string>

namespace md::test {
namespace {

std::size_t occurrenceCount(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

BookManagerSnapshot snapshot(std::size_t event_count) {
    BookManagerSnapshot result;
    result.event_count = event_count;
    result.timestamp = 100 + event_count;
    result.processed_events = event_count;
    result.unresolved_events = 0;
    result.instruments.push_back(InstrumentBookSnapshot{
        .instrument_id = 1,
        .resting_orders = 1,
        .best_bid = 100'000'000'000LL,
        .best_ask = 105'000'000'000LL,
        .bids = {PriceLevelSnapshot{.price = 100'000'000'000LL, .size = 10}},
        .asks = {PriceLevelSnapshot{.price = 105'000'000'000LL, .size = 7}},
    });
    return result;
}

} // namespace

void testAsyncSnapshotWriterWritesAllJobs() {
    std::ostringstream out;
    AsyncSnapshotWriter writer{out, SnapshotWriterMode::Async};

    writer.write(snapshot(1));
    writer.write(snapshot(2));
    writer.finish();

    const auto text = out.str();
    require(writer.submittedCount() == 2, "async snapshot writer submitted count");
    require(writer.writtenCount() == 2, "async snapshot writer written count");
    require(occurrenceCount(text, "LOB Snapshot") == 2, "async snapshot writer writes all jobs");
    requireContains(text, "event_count=1", "async snapshot writer first event");
    requireContains(text, "event_count=2", "async snapshot writer second event");
}

void testAsyncSnapshotWriterFinishDrainsQueue() {
    std::ostringstream out;
    AsyncSnapshotWriter writer{out, SnapshotWriterMode::Async};

    for (std::size_t i = 1; i <= 25; ++i) {
        writer.write(snapshot(i));
    }
    writer.finish();

    require(writer.submittedCount() == 25, "async snapshot writer drain submitted count");
    require(writer.writtenCount() == 25, "async snapshot writer finish drains queue");
    require(occurrenceCount(out.str(), "LOB Snapshot") == 25, "async snapshot writer drain output count");
}

} // namespace md::test
