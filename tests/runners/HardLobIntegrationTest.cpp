#include "TestSupport.hpp"

#include "processing/LobMarketDataEventProcessor.hpp"
#include "processing/ShardedLobMarketDataEventProcessor.hpp"
#include "runners/BenchmarkRunner.hpp"
#include "runners/FlatMergeRunner.hpp"
#include "runners/HierarchicalMergeRunner.hpp"
#include "runners/ResultPrinter.hpp"
#include "runners/RunResult.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace md::test {
namespace {

std::int64_t P(std::int64_t integer_price) {
    return integer_price * 1'000'000'000LL;
}

LobProcessorConfig hardLobConfig(SnapshotWriterMode snapshot_writer_mode = SnapshotWriterMode::Sync) {
    LobProcessorConfig config;
    config.snapshot_interval_events = 3;
    config.max_snapshots = 2;
    config.snapshot_depth = 5;
    config.snapshot_writer_mode = snapshot_writer_mode;
    return config;
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

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> items;
    std::string item;
    std::istringstream input{text};
    while (std::getline(input, item, delimiter)) {
        items.push_back(item);
    }
    return items;
}

void requireExpectedHardLobResult(
    const RunResult& result,
    const BookManager& books,
    const std::string& output,
    const std::string& strategy_name
) {
    require(result.strategy_name == strategy_name, strategy_name + " lob strategy name");
    require(result.summary.total_messages_processed == 7, strategy_name + " lob processed events");
    require(result.diagnostics.total_lines_read == 7, strategy_name + " lob lines read");
    require(result.summary.chronological_violations == 0, strategy_name + " lob chronological violations");

    require(books.instrumentCount() == 2, strategy_name + " lob instrument count");
    require(books.processedEvents() == 7, strategy_name + " lob book manager processed events");
    require(books.unresolvedEvents() == 0, strategy_name + " lob unresolved events");

    const auto* first = books.findBook(1);
    require(first != nullptr, strategy_name + " lob instrument 1 exists");
    require(first->bestBid() == P(100), strategy_name + " lob instrument 1 best bid");
    require(first->bestAsk() == P(104), strategy_name + " lob instrument 1 best ask");
    require(first->volumeAt(Side::Bid, P(100)) == 6, strategy_name + " lob instrument 1 bid volume");
    require(first->volumeAt(Side::Ask, P(104)) == 5, strategy_name + " lob instrument 1 ask volume");

    const auto* second = books.findBook(2);
    require(second != nullptr, strategy_name + " lob instrument 2 exists");
    require(second->bestBid() == P(200), strategy_name + " lob instrument 2 best bid");
    require(second->bestAsk() == P(210), strategy_name + " lob instrument 2 best ask");
    require(second->volumeAt(Side::Bid, P(200)) == 12, strategy_name + " lob instrument 2 bid volume");
    require(second->volumeAt(Side::Ask, P(210)) == 5, strategy_name + " lob instrument 2 ask volume");

    require(occurrenceCount(output, "LOB Snapshot") == 2, strategy_name + " lob snapshot count");
    requireContains(output, "event_count=3", strategy_name + " lob snapshot at event 3");
    requireContains(output, "event_count=6", strategy_name + " lob snapshot at event 6");
}

std::filesystem::path hardLobSyntheticDir() {
    return testDataDir() / "hard_lob_synthetic";
}

std::filesystem::path hardLobEqualTimestampsDir() {
    return testDataDir() / "hard_lob_equal_timestamps";
}

std::vector<BenchmarkResult> runSyntheticLobBenchmark() {
    std::ostringstream err;
    return runLobBenchmark(hardLobSyntheticDir(), InputFormat::Json, false, err);
}

std::vector<BenchmarkResult> runSyntheticShardedLobBenchmark(std::size_t worker_count) {
    std::ostringstream err;
    return runLobBenchmark(hardLobSyntheticDir(), InputFormat::Json, false, err, worker_count);
}

std::vector<BenchmarkResult> runSyntheticLoggingBenchmark() {
    std::ostringstream err;
    return runLoggingBenchmark(hardLobSyntheticDir(), InputFormat::Json, false, err);
}

std::string printSyntheticLobBenchmark(const std::vector<BenchmarkResult>& results) {
    std::ostringstream out;
    printLobBenchmarkResults(results, out);
    return out.str();
}

std::string printSyntheticLoggingBenchmark(const std::vector<BenchmarkResult>& results) {
    std::ostringstream out;
    printBenchmarkResults(results, out);
    return out.str();
}

std::vector<std::vector<std::string>> benchmarkCsvRows(const std::string& output) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream input{output};
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            rows.push_back(split(line, ','));
        }
    }
    return rows;
}

struct LobRun {
    RunResult result;
    BookManager books;
    std::string output;
    std::size_t snapshot_written_count{};
};

struct ShardedLobRun {
    RunResult result;
    std::string digest;
    std::string output;
    std::size_t unresolved_events{};
    std::size_t processed_count{};
};

LobRun runFlatLob(
    const std::filesystem::path& input_dir,
    SnapshotWriterMode snapshot_writer_mode = SnapshotWriterMode::Sync
) {
    std::ostringstream out;
    std::ostringstream err;
    LobMarketDataEventProcessor processor{out, hardLobConfig(snapshot_writer_mode)};
    auto result = FlatMergeRunner{}.run(input_dir, processor, true, err);
    processor.finishSnapshots();
    return LobRun{std::move(result), processor.books(), out.str(), processor.snapshotWrittenCount()};
}

LobRun runFlatLobNoSnapshots(const std::filesystem::path& input_dir) {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.snapshot_interval_events = 0;
    config.max_snapshots = 0;
    LobMarketDataEventProcessor processor{out, config};
    auto result = FlatMergeRunner{}.run(input_dir, processor, false, err);
    processor.finishSnapshots();
    return LobRun{std::move(result), processor.books(), out.str(), processor.snapshotWrittenCount()};
}

ShardedLobRun runFlatShardedLob(
    const std::filesystem::path& input_dir,
    std::size_t worker_count
) {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.snapshot_interval_events = 0;
    config.max_snapshots = 0;
    ShardedLobMarketDataEventProcessor processor{out, worker_count, config};
    auto result = FlatMergeRunner{}.run(input_dir, processor, false, err);
    processor.finish();
    return ShardedLobRun{
        .result = std::move(result),
        .digest = processor.stableStateDigest(),
        .output = out.str(),
        .unresolved_events = processor.unresolvedEvents(),
        .processed_count = processor.processedCount(),
    };
}

ShardedLobRun runHierarchyShardedLob(
    const std::filesystem::path& input_dir,
    std::size_t worker_count
) {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.snapshot_interval_events = 0;
    config.max_snapshots = 0;
    ShardedLobMarketDataEventProcessor processor{out, worker_count, config};
    auto result = HierarchicalMergeRunner{}.run(input_dir, processor, false, err);
    processor.finish();
    return ShardedLobRun{
        .result = std::move(result),
        .digest = processor.stableStateDigest(),
        .output = out.str(),
        .unresolved_events = processor.unresolvedEvents(),
        .processed_count = processor.processedCount(),
    };
}

LobRun runHierarchyLob(
    const std::filesystem::path& input_dir,
    SnapshotWriterMode snapshot_writer_mode = SnapshotWriterMode::Sync
) {
    std::ostringstream out;
    std::ostringstream err;
    LobMarketDataEventProcessor processor{out, hardLobConfig(snapshot_writer_mode)};
    auto result = HierarchicalMergeRunner{}.run(input_dir, processor, true, err);
    processor.finishSnapshots();
    return LobRun{std::move(result), processor.books(), out.str(), processor.snapshotWrittenCount()};
}

} // namespace

void testFlatRunnerWithLobProcessorReconstructsExpectedBooks() {
    std::ostringstream out;
    std::ostringstream err;
    LobMarketDataEventProcessor processor{out, hardLobConfig()};

    const auto result = FlatMergeRunner{}.run(hardLobSyntheticDir(), processor, true, err);

    requireExpectedHardLobResult(result, processor.books(), out.str(), "flat");
    requireContains(err.str(), "selected_mode=flat", "flat lob verbose mode logged");
}

void testHierarchyRunnerWithLobProcessorReconstructsExpectedBooks() {
    std::ostringstream out;
    std::ostringstream err;
    LobMarketDataEventProcessor processor{out, hardLobConfig()};

    const auto result = HierarchicalMergeRunner{}.run(hardLobSyntheticDir(), processor, true, err);

    requireExpectedHardLobResult(result, processor.books(), out.str(), "hierarchy");
    requireContains(err.str(), "selected_mode=hierarchy", "hierarchy lob verbose mode logged");
}

void testFlatAndHierarchyLobFinalBooksAreIdentical() {
    auto flat = runFlatLob(hardLobSyntheticDir());
    auto hierarchy = runHierarchyLob(hardLobSyntheticDir());

    requireExpectedHardLobResult(flat.result, flat.books, flat.output, "flat");
    requireExpectedHardLobResult(hierarchy.result, hierarchy.books, hierarchy.output, "hierarchy");
    require(
        flat.books.stableStateDigest() == hierarchy.books.stableStateDigest(),
        "flat and hierarchy final LOB digests are identical"
    );
}

void testFlatAndHierarchyLobSyntheticHaveSameMessageCount() {
    auto flat = runFlatLob(hardLobSyntheticDir());
    auto hierarchy = runHierarchyLob(hardLobSyntheticDir());

    require(
        flat.result.summary.total_messages_processed == hierarchy.result.summary.total_messages_processed,
        "flat and hierarchy synthetic lob message counts match"
    );
    require(flat.result.summary.total_messages_processed == 7, "synthetic lob message count is expected");
}

void testFlatAndHierarchyLobSyntheticHaveZeroChronologicalViolations() {
    auto flat = runFlatLob(hardLobSyntheticDir());
    auto hierarchy = runHierarchyLob(hardLobSyntheticDir());

    require(flat.result.summary.chronological_violations == 0, "flat synthetic lob chronological violations");
    require(hierarchy.result.summary.chronological_violations == 0, "hierarchy synthetic lob chronological violations");
}

void testFlatAndHierarchyLobSyntheticHaveSameFinalLobDigest() {
    auto flat = runFlatLob(hardLobSyntheticDir());
    auto hierarchy = runHierarchyLob(hardLobSyntheticDir());

    const auto flat_digest = flat.books.stableStateDigest();
    const auto hierarchy_digest = hierarchy.books.stableStateDigest();
    require(flat_digest == hierarchy_digest, "flat and hierarchy synthetic lob stable digest");
    requireContains(flat_digest, "processed_events=7", "digest includes processed events");
    requireContains(flat_digest, "unresolved_events=0", "digest includes unresolved events");
    requireContains(flat_digest, "instrument=1", "digest includes instrument 1");
    requireContains(flat_digest, "bids=[100.000000000x6]", "digest includes bid levels");
    requireContains(flat_digest, "asks=[104.000000000x5]", "digest includes ask levels");
    requireContains(flat_digest, "instrument=2", "digest includes instrument 2");
    requireContains(flat_digest, "bids=[200.000000000x12]", "digest includes resolved missing instrument cancel");
}

void testFlatAndHierarchyLobEqualTimestampsAreDeterministic() {
    auto flat = runFlatLob(hardLobEqualTimestampsDir());
    auto hierarchy = runHierarchyLob(hardLobEqualTimestampsDir());

    CapturingProcessor flat_capture;
    std::ostringstream flat_err;
    const auto flat_capture_result = FlatMergeRunner{}.run(
        hardLobEqualTimestampsDir(),
        flat_capture,
        false,
        flat_err
    );

    CapturingProcessor hierarchy_capture;
    std::ostringstream hierarchy_err;
    const auto hierarchy_capture_result = HierarchicalMergeRunner{}.run(
        hardLobEqualTimestampsDir(),
        hierarchy_capture,
        false,
        hierarchy_err
    );

    require(flat.result.summary.total_messages_processed == 2, "flat equal timestamp message count");
    require(hierarchy.result.summary.total_messages_processed == 2, "hierarchy equal timestamp message count");
    require(flat_capture_result.summary.total_messages_processed == 2, "flat equal timestamp capture count");
    require(hierarchy_capture_result.summary.total_messages_processed == 2, "hierarchy equal timestamp capture count");
    require(flat.result.summary.chronological_violations == 0, "flat equal timestamp chronological violations");
    require(hierarchy.result.summary.chronological_violations == 0, "hierarchy equal timestamp chronological violations");
    require(flat_capture.events.size() == hierarchy_capture.events.size(), "equal timestamp captured event counts match");
    require(flat_capture.events.size() == 2, "equal timestamp captured two events");
    for (std::size_t i = 0; i < flat_capture.events.size(); ++i) {
        require(
            flat_capture.events[i].order_id == hierarchy_capture.events[i].order_id,
            "equal timestamp flat and hierarchy event order match"
        );
        require(
            flat_capture.events[i].source_file_id == hierarchy_capture.events[i].source_file_id,
            "equal timestamp flat and hierarchy source file order match"
        );
    }
    require(flat_capture.events[0].order_id == 1, "equal timestamp first event uses lower source file id");
    require(flat_capture.events[1].order_id == 2, "equal timestamp second event uses higher source file id");
    require(
        flat.books.stableStateDigest() == hierarchy.books.stableStateDigest(),
        "equal timestamp final digest is deterministic across flat and hierarchy"
    );
}

void testFlatLobAsyncSnapshotsMatchSyncOnSyntheticFolder() {
    const auto sync = runFlatLob(hardLobSyntheticDir(), SnapshotWriterMode::Sync);
    const auto async = runFlatLob(hardLobSyntheticDir(), SnapshotWriterMode::Async);

    require(sync.result.summary.total_messages_processed == 7, "sync synthetic folder message count");
    require(async.result.summary.total_messages_processed == 7, "async synthetic folder message count");
    require(sync.result.summary.chronological_violations == 0, "sync synthetic folder chronological violations");
    require(async.result.summary.chronological_violations == 0, "async synthetic folder chronological violations");
    require(sync.books.unresolvedEvents() == 0, "sync synthetic folder unresolved events");
    require(async.books.unresolvedEvents() == 0, "async synthetic folder unresolved events");
    require(sync.snapshot_written_count == 2, "sync synthetic folder written snapshots");
    require(async.snapshot_written_count == 2, "async synthetic folder written snapshots");
    require(
        sync.books.stableStateDigest() == async.books.stableStateDigest(),
        "sync and async synthetic folder final LOB digest match"
    );
    require(
        occurrenceCount(sync.output, "LOB Snapshot") == occurrenceCount(async.output, "LOB Snapshot"),
        "sync and async synthetic folder snapshot count match"
    );
}

void testBenchmarkLobRunsFlatAndHierarchy() {
    const auto results = runSyntheticLobBenchmark();
    const auto output = printSyntheticLobBenchmark(results);

    require(results.size() == 2, "lob benchmark produces two results");
    require(results[0].result.strategy_name == "flat", "lob benchmark flat row");
    require(results[1].result.strategy_name == "hierarchy", "lob benchmark hierarchy row");
    requireContains(output, "Benchmark LOB", "lob benchmark title");
    requireContains(output, "Strategy,InputFormat,Processor", "lob benchmark extended header");
    requireContains(output, "flat,json,lob,7,0,0,", "lob benchmark flat csv row");
    requireContains(output, "hierarchy,json,lob,7,0,0,", "lob benchmark hierarchy csv row");
    requireContains(output, "LobDigest", "lob benchmark digest header");
}

void testBenchmarkLoggingRunsFlatAndHierarchy() {
    const auto results = runSyntheticLoggingBenchmark();
    const auto output = printSyntheticLoggingBenchmark(results);

    require(results.size() == 2, "logging benchmark produces two results");
    require(results[0].result.strategy_name == "flat", "logging benchmark flat row");
    require(results[1].result.strategy_name == "hierarchy", "logging benchmark hierarchy row");
    requireContains(output, "Benchmark", "logging benchmark title");
    requireContains(output, "Strategy,InputFormat,Processor", "logging benchmark extended header");
    requireContains(output, "flat,json,logging,7,0,0,", "logging benchmark flat csv row");
    requireContains(output, "hierarchy,json,logging,7,0,0,", "logging benchmark hierarchy csv row");
    require(output.find("LobDigest") == std::string::npos, "logging benchmark does not print digest column");
}

void testBenchmarkLobSuppressesSnapshotsByDefault() {
    const auto output = printSyntheticLobBenchmark(runSyntheticLobBenchmark());

    require(output.find("LOB Snapshot") == std::string::npos, "lob benchmark suppresses snapshots");
    require(output.find("BookManager snapshot") == std::string::npos, "lob benchmark suppresses book snapshots");
}

void testBenchmarkLoggingOutputsParseableReportRows() {
    const auto results = runSyntheticLoggingBenchmark();
    const auto output = printSyntheticLoggingBenchmark(results);
    const auto rows = benchmarkCsvRows(output);

    require(results.size() == 2, "logging benchmark result size");
    require(rows.size() == 4, "logging benchmark has title header and two rows");
    require(rows[2].size() == 8, "logging benchmark flat parseable csv columns");
    require(rows[3].size() == 8, "logging benchmark hierarchy parseable csv columns");

    require(std::stoull(rows[2][3]) == 7, "logging benchmark flat messages parse");
    require(std::stoull(rows[3][3]) == 7, "logging benchmark hierarchy messages parse");
    require(std::stoull(rows[2][4]) == 0, "logging benchmark flat chronological parse");
    require(std::stoull(rows[3][4]) == 0, "logging benchmark hierarchy chronological parse");
    require(std::stoull(rows[2][5]) == 0, "logging benchmark flat unresolved parse");
    require(std::stoull(rows[3][5]) == 0, "logging benchmark hierarchy unresolved parse");
    require(std::stod(rows[2][6]) > 0.0, "logging benchmark flat wall clock parse");
    require(std::stod(rows[3][6]) > 0.0, "logging benchmark hierarchy wall clock parse");
    require(std::stod(rows[2][7]) > 0.0, "logging benchmark flat throughput parse");
    require(std::stod(rows[3][7]) > 0.0, "logging benchmark hierarchy throughput parse");
}

void testBenchmarkLobOutputsEqualMessageCounts() {
    const auto results = runSyntheticLobBenchmark();
    const auto output = printSyntheticLobBenchmark(results);
    const auto rows = benchmarkCsvRows(output);

    require(results.size() == 2, "lob benchmark message count result size");
    require(rows.size() == 4, "lob benchmark has title header and two rows");
    require(rows[2].size() == 9, "lob benchmark flat parseable csv columns");
    require(rows[3].size() == 9, "lob benchmark hierarchy parseable csv columns");

    require(results[0].result.summary.total_messages_processed == results[1].result.summary.total_messages_processed,
        "lob benchmark message counts match");
    require(results[0].result.summary.total_messages_processed == 7, "lob benchmark synthetic message count");
    require(results[0].result.summary.chronological_violations == 0, "lob benchmark flat chronological violations");
    require(results[1].result.summary.chronological_violations == 0, "lob benchmark hierarchy chronological violations");
    require(results[0].unresolved_events == results[1].unresolved_events, "lob benchmark unresolved counts match");

    require(std::stoull(rows[2][3]) == 7, "lob benchmark flat messages parse");
    require(std::stoull(rows[3][3]) == 7, "lob benchmark hierarchy messages parse");
    require(std::stoull(rows[2][4]) == 0, "lob benchmark flat chronological parse");
    require(std::stoull(rows[3][4]) == 0, "lob benchmark hierarchy chronological parse");
    require(std::stoull(rows[2][5]) == 0, "lob benchmark flat unresolved parse");
    require(std::stoull(rows[3][5]) == 0, "lob benchmark hierarchy unresolved parse");
    require(std::stod(rows[2][6]) > 0.0, "lob benchmark flat wall clock parse");
    require(std::stod(rows[3][6]) > 0.0, "lob benchmark hierarchy wall clock parse");
    require(std::stod(rows[2][7]) > 0.0, "lob benchmark flat throughput parse");
    require(std::stod(rows[3][7]) > 0.0, "lob benchmark hierarchy throughput parse");
}

void testBenchmarkLobOutputsEqualFinalLobDigest() {
    const auto results = runSyntheticLobBenchmark();
    const auto output = printSyntheticLobBenchmark(results);
    const auto rows = benchmarkCsvRows(output);

    require(results.size() == 2, "lob benchmark digest result size");
    require(results[0].lob_digest == results[1].lob_digest, "lob benchmark raw final LOB digest matches");
    require(rows.size() == 4, "lob benchmark digest output rows");
    require(rows[2].size() == 9, "lob benchmark flat digest columns");
    require(rows[3].size() == 9, "lob benchmark hierarchy digest columns");
    require(rows[2][8] == rows[3][8], "lob benchmark printed digest matches");
    require(rows[2][8].starts_with("0x"), "lob benchmark digest is hex fingerprint");
}

void testBenchmarkLobLabelsShardedWorkers() {
    const auto results = runSyntheticShardedLobBenchmark(2);
    const auto output = printSyntheticLobBenchmark(results);

    require(results.size() == 2, "sharded lob benchmark result size");
    require(results[0].processor == "lob-sharded-2", "sharded lob benchmark flat processor label");
    require(results[1].processor == "lob-sharded-2", "sharded lob benchmark hierarchy processor label");
    requireContains(output, "flat,json,lob-sharded-2,7,0,0,", "sharded lob benchmark flat row");
    requireContains(output, "hierarchy,json,lob-sharded-2,7,0,0,", "sharded lob benchmark hierarchy row");
    require(results[0].lob_digest == runSyntheticLobBenchmark()[0].lob_digest, "sharded benchmark digest matches sequential");
}

void testShardedLobTwoWorkersMatchesSequentialDigest() {
    const auto sequential = runFlatLobNoSnapshots(hardLobSyntheticDir());
    const auto sharded = runFlatShardedLob(hardLobSyntheticDir(), 2);

    require(sharded.result.summary.total_messages_processed == 7, "two-worker sharded processed events");
    require(sharded.result.summary.chronological_violations == 0, "two-worker sharded chronological violations");
    require(sharded.unresolved_events == 0, "two-worker sharded unresolved events");
    require(sharded.digest == sequential.books.stableStateDigest(), "two-worker sharded digest matches sequential");
}

void testShardedLobFourWorkersMatchesSequentialDigest() {
    const auto sequential = runFlatLobNoSnapshots(hardLobSyntheticDir());
    const auto sharded = runFlatShardedLob(hardLobSyntheticDir(), 4);

    require(sharded.result.summary.total_messages_processed == 7, "four-worker sharded processed events");
    require(sharded.result.summary.chronological_violations == 0, "four-worker sharded chronological violations");
    require(sharded.unresolved_events == 0, "four-worker sharded unresolved events");
    require(sharded.digest == sequential.books.stableStateDigest(), "four-worker sharded digest matches sequential");
}

void testShardedLobResolvesMissingInstrumentIdByOrderId() {
    const auto sharded = runFlatShardedLob(hardLobSyntheticDir(), 2);

    require(sharded.unresolved_events == 0, "sharded resolves missing instrument id");
    requireContains(
        sharded.digest,
        "instrument=2,orders=2,best_bid=200.000000000,best_ask=210.000000000,bids=[200.000000000x12]",
        "sharded digest includes missing-instrument cancel applied to instrument 2"
    );
}

void testShardedLobPreservesPerInstrumentOrder() {
    const auto sequential = runFlatLobNoSnapshots(hardLobSyntheticDir());
    const auto flat_sharded = runFlatShardedLob(hardLobSyntheticDir(), 4);
    const auto hierarchy_sharded = runHierarchyShardedLob(hardLobSyntheticDir(), 4);

    require(flat_sharded.processed_count == 7, "flat sharded processed count");
    require(hierarchy_sharded.processed_count == 7, "hierarchy sharded processed count");
    require(flat_sharded.digest == sequential.books.stableStateDigest(), "flat sharded preserves per-instrument order");
    require(hierarchy_sharded.digest == sequential.books.stableStateDigest(), "hierarchy sharded preserves per-instrument order");
}

void testShardedLobUnknownOrderWithoutInstrumentGoesToUnresolvedCounter() {
    std::ostringstream out;
    LobProcessorConfig config;
    config.snapshot_interval_events = 0;
    config.max_snapshots = 0;
    ShardedLobMarketDataEventProcessor processor{out, 2, config};

    processor.processMarketDataEvent(MarketDataEvent{
        .timestamp = 100,
        .order_id = 999,
        .size = 1,
        .action = Action::Cancel,
        .instrument_id = 0,
    });
    processor.finish();

    require(processor.processedCount() == 1, "unknown missing instrument sharded processed count");
    require(processor.unresolvedEvents() == 1, "unknown missing instrument sharded unresolved count");
    requireContains(processor.stableStateDigest(), "unresolved_events=1", "unknown missing instrument digest");
}

} // namespace md::test
