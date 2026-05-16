#include "TestSupport.hpp"

#include "processing/LobMarketDataEventProcessor.hpp"

#include <sstream>
#include <string>

namespace md::test {

namespace {

std::int64_t P(std::int64_t integer_price) {
    return integer_price * 1'000'000'000LL;
}

MarketDataEvent event(
    Action action,
    std::uint64_t instrument_id,
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    MarketDataEvent result;
    result.action = action;
    result.instrument_id = instrument_id;
    result.order_id = order_id;
    result.side = side;
    result.price = price;
    result.size = size;
    return result;
}

MarketDataEvent add(
    std::uint64_t instrument_id,
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    return event(Action::Add, instrument_id, order_id, side, price, size);
}

MarketDataEvent modify(
    std::uint64_t instrument_id,
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    return event(Action::Modify, instrument_id, order_id, side, price, size);
}

std::size_t occurrenceCount(const std::string& text, const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

void testLobProcessorUpdatesBooks() {
    std::ostringstream out;
    LobMarketDataEventProcessor processor{out};

    processor.processMarketDataEvent(add(1, 1, Side::Bid, P(100), 10));
    processor.processMarketDataEvent(add(1, 2, Side::Ask, P(105), 7));
    processor.processMarketDataEvent(modify(1, 1, Side::Bid, P(101), 8));

    const auto& books = processor.books();
    const auto* book = books.findBook(1);
    require(book != nullptr, "lob processor created instrument book");
    require(processor.processedCount() == 3, "lob processor processed count");
    require(books.instrumentCount() == 1, "lob processor instrument count");
    require(book->bestBid() == P(101), "lob processor best bid");
    require(book->bestAsk() == P(105), "lob processor best ask");
    require(book->volumeAt(Side::Bid, P(101)) == 8, "lob processor bid volume");
    require(book->volumeAt(Side::Ask, P(105)) == 7, "lob processor ask volume");
}

void testLobProcessorPrintsSnapshotsAtInterval() {
    std::ostringstream out;
    LobProcessorConfig config;
    config.snapshot_interval_events = 2;
    config.max_snapshots = 2;
    config.snapshot_depth = 2;
    LobMarketDataEventProcessor processor{out, config};

    processor.processMarketDataEvent(add(1, 1, Side::Bid, P(100), 10));
    processor.processMarketDataEvent(add(1, 2, Side::Bid, P(101), 10));
    processor.processMarketDataEvent(add(1, 3, Side::Ask, P(105), 7));
    processor.processMarketDataEvent(add(1, 4, Side::Ask, P(106), 7));

    const auto text = out.str();
    requireContains(text, "LOB Snapshot", "snapshot output contains marker");
    requireContains(text, "event_count=2", "snapshot emitted at event 2");
    requireContains(text, "event_count=4", "snapshot emitted at event 4");
    require(text.find("event_count=1") == std::string::npos, "snapshot not emitted at event 1");
    require(text.find("event_count=3") == std::string::npos, "snapshot not emitted at event 3");
}

void testLobProcessorRespectsMaxSnapshots() {
    std::ostringstream out;
    LobProcessorConfig config;
    config.snapshot_interval_events = 1;
    config.max_snapshots = 2;
    LobMarketDataEventProcessor processor{out, config};

    for (std::uint64_t i = 1; i <= 5; ++i) {
        processor.processMarketDataEvent(add(1, i, Side::Bid, P(100), 10));
    }

    require(occurrenceCount(out.str(), "LOB Snapshot") == 2, "processor respects max snapshots");
    require(processor.snapshotCount() == 2, "processor snapshot count");
}

void testLobProcessorFinalSummaryContainsBestBidAsk() {
    std::ostringstream out;
    LobMarketDataEventProcessor processor{out};

    processor.processMarketDataEvent(add(1, 1, Side::Bid, P(100), 10));
    processor.processMarketDataEvent(add(1, 2, Side::Ask, P(105), 7));
    processor.processMarketDataEvent(add(2, 3, Side::Bid, P(200), 20));
    processor.printFinalSummary();

    const auto text = out.str();
    requireContains(text, "Final LOB Summary", "final summary marker");
    requireContains(text, "instrument_count=2", "final summary instrument count");
    requireContains(text, "processed_events=3", "final summary processed events");
    requireContains(text, "unresolved_events=0", "final summary unresolved events");
    requireContains(text, "instrument_id=1", "final summary first instrument");
    requireContains(text, "resting_orders=2", "final summary first resting orders");
    requireContains(text, "best_bid=100.000000000", "final summary first best bid");
    requireContains(text, "best_ask=105.000000000", "final summary first best ask");
    requireContains(text, "instrument_id=2", "final summary second instrument");
    requireContains(text, "resting_orders=1", "final summary second resting orders");
    requireContains(text, "best_bid=200.000000000", "final summary second best bid");
    requireContains(text, "best_ask=<none>", "final summary second missing best ask");
}

void testLobProcessorAsyncSnapshotsDoNotChangeFinalLobDigest() {
    LobProcessorConfig sync_config;
    sync_config.snapshot_interval_events = 2;
    sync_config.max_snapshots = 2;
    sync_config.snapshot_depth = 2;
    sync_config.snapshot_writer_mode = SnapshotWriterMode::Sync;

    LobProcessorConfig async_config = sync_config;
    async_config.snapshot_writer_mode = SnapshotWriterMode::Async;

    std::ostringstream sync_out;
    LobMarketDataEventProcessor sync_processor{sync_out, sync_config};
    std::ostringstream async_out;
    LobMarketDataEventProcessor async_processor{async_out, async_config};

    const auto events = {
        add(1, 1, Side::Bid, P(100), 10),
        add(1, 2, Side::Ask, P(105), 7),
        modify(1, 1, Side::Bid, P(101), 8),
        add(2, 3, Side::Bid, P(200), 20),
    };

    for (const auto& item : events) {
        sync_processor.processMarketDataEvent(item);
        async_processor.processMarketDataEvent(item);
    }

    sync_processor.finishSnapshots();
    async_processor.finishSnapshots();

    require(
        sync_processor.books().stableStateDigest() == async_processor.books().stableStateDigest(),
        "async snapshots do not change final LOB digest"
    );
    require(sync_processor.processedCount() == 4, "sync processor processed count");
    require(async_processor.processedCount() == 4, "async processor processed count");
    require(async_processor.snapshotWrittenCount() == 2, "async processor written snapshot count");
}

void testSyncAndAsyncSnapshotOutputsHaveSameSnapshotCount() {
    LobProcessorConfig sync_config;
    sync_config.snapshot_interval_events = 2;
    sync_config.max_snapshots = 2;
    sync_config.snapshot_depth = 2;
    sync_config.snapshot_writer_mode = SnapshotWriterMode::Sync;

    LobProcessorConfig async_config = sync_config;
    async_config.snapshot_writer_mode = SnapshotWriterMode::Async;

    std::ostringstream sync_out;
    LobMarketDataEventProcessor sync_processor{sync_out, sync_config};
    std::ostringstream async_out;
    LobMarketDataEventProcessor async_processor{async_out, async_config};

    for (std::uint64_t i = 1; i <= 5; ++i) {
        const auto item = add(1, i, Side::Bid, P(100 + static_cast<std::int64_t>(i)), 10);
        sync_processor.processMarketDataEvent(item);
        async_processor.processMarketDataEvent(item);
    }

    sync_processor.finishSnapshots();
    async_processor.finishSnapshots();

    require(
        occurrenceCount(sync_out.str(), "LOB Snapshot") == occurrenceCount(async_out.str(), "LOB Snapshot"),
        "sync and async snapshot outputs have same snapshot count"
    );
    require(occurrenceCount(async_out.str(), "LOB Snapshot") == 2, "async snapshot output count");
}

} // namespace md::test
