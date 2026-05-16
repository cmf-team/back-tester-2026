#include "TestSupport.hpp"

#include "processing/LobMarketDataEventProcessor.hpp"
#include "runners/ResultPrinter.hpp"
#include "runners/StandardRunner.hpp"

#include <sstream>
#include <string>

namespace md::test {
namespace {

std::int64_t P(std::int64_t integer_price) {
    return integer_price * 1'000'000'000LL;
}

std::string syntheticLobInput() {
    return
        "{\"ts_recv\":100,\"ts_event\":100,\"instrument_id\":1,\"order_id\":1,\"side\":\"B\",\"price\":100000000000,\"size\":10,\"action\":\"A\"}\n"
        "{\"ts_recv\":101,\"ts_event\":101,\"instrument_id\":1,\"order_id\":2,\"side\":\"A\",\"price\":105000000000,\"size\":7,\"action\":\"A\"}\n"
        "{\"ts_recv\":102,\"ts_event\":102,\"instrument_id\":2,\"order_id\":3,\"side\":\"B\",\"price\":200000000000,\"size\":20,\"action\":\"A\"}\n"
        "{\"ts_recv\":103,\"ts_event\":103,\"instrument_id\":1,\"order_id\":1,\"side\":\"B\",\"price\":101000000000,\"size\":8,\"action\":\"M\"}\n"
        "{\"ts_recv\":104,\"ts_event\":104,\"instrument_id\":1,\"order_id\":2,\"size\":3,\"action\":\"C\"}\n"
        "{\"ts_recv\":105,\"ts_event\":105,\"instrument_id\":1,\"order_id\":4,\"side\":\"B\",\"price\":99000000000,\"size\":5,\"action\":\"A\"}\n"
        "{\"ts_recv\":106,\"ts_event\":106,\"instrument_id\":1,\"order_id\":999,\"side\":\"B\",\"price\":102000000000,\"size\":1,\"action\":\"T\"}\n"
        "{\"ts_recv\":107,\"ts_event\":107,\"instrument_id\":1,\"order_id\":1,\"size\":2,\"action\":\"F\"}\n";
}

std::filesystem::path writeSyntheticLobFile(const std::filesystem::path& dir) {
    const auto file = dir / "lob_standard_synthetic.ndjson";
    writeFile(file, syntheticLobInput());
    return file;
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

double lineValueAfter(const std::string& text, const std::string& prefix) {
    const auto pos = text.find(prefix);
    if (pos == std::string::npos) {
        throw std::runtime_error("missing output line: " + prefix);
    }

    const auto value_begin = pos + prefix.size();
    const auto value_end = text.find('\n', value_begin);
    return std::stod(text.substr(value_begin, value_end - value_begin));
}

} // namespace

void testStandardRunnerWithLobProcessorReconstructsExpectedBooks() {
    const auto dir = makeTempDir("standard_lob_reconstructs");
    const auto file = writeSyntheticLobFile(dir);

    std::ostringstream out;
    std::ostringstream err;
    LobMarketDataEventProcessor processor{out};
    const auto result = StandardRunner{}.run(file, processor, false, err);

    require(result.strategy_name == "standard", "lob standard strategy name");
    require(result.summary.total_messages_processed == 8, "lob standard message count");
    require(result.summary.chronological_violations == 0, "lob standard chronological violations");
    require(result.summary.first_timestamp == 100, "lob standard first timestamp");
    require(result.summary.last_timestamp == 107, "lob standard last timestamp");

    const auto& books = processor.books();
    require(books.instrumentCount() == 2, "lob standard instrument count");
    require(books.processedEvents() == 8, "lob standard book manager processed count");

    const auto* first = books.findBook(1);
    require(first != nullptr, "lob standard instrument 1 book exists");
    require(first->bestBid() == P(101), "lob standard instrument 1 best bid");
    require(first->bestAsk() == P(105), "lob standard instrument 1 best ask");
    require(first->volumeAt(Side::Bid, P(101)) == 8, "lob standard instrument 1 bid 101 volume");
    require(first->volumeAt(Side::Bid, P(99)) == 5, "lob standard instrument 1 bid 99 volume");
    require(first->volumeAt(Side::Ask, P(105)) == 4, "lob standard instrument 1 ask 105 volume");
    require(first->restingOrderCount() == 3, "lob standard instrument 1 resting orders");

    const auto* second = books.findBook(2);
    require(second != nullptr, "lob standard instrument 2 book exists");
    require(second->bestBid() == P(200), "lob standard instrument 2 best bid");
    require(!second->bestAsk().has_value(), "lob standard instrument 2 no best ask");
    require(second->volumeAt(Side::Bid, P(200)) == 20, "lob standard instrument 2 bid volume");
    require(second->restingOrderCount() == 1, "lob standard instrument 2 resting orders");

    std::filesystem::remove_all(dir);
}

void testStandardRunnerWithLobProcessorPrintsExpectedSnapshots() {
    const auto dir = makeTempDir("standard_lob_snapshots");
    const auto file = writeSyntheticLobFile(dir);

    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.snapshot_interval_events = 4;
    config.max_snapshots = 2;
    config.snapshot_depth = 2;
    LobMarketDataEventProcessor processor{out, config};

    StandardRunner{}.run(file, processor, false, err);

    const auto text = out.str();
    require(occurrenceCount(text, "LOB Snapshot") == 2, "lob standard snapshot count");
    requireContains(text, "event_count=4", "lob standard snapshot at event 4");
    requireContains(text, "event_count=8", "lob standard snapshot at event 8");
    requireContains(text, "instrument_id=1", "lob standard snapshot includes instrument 1");
    requireContains(text, "best_bid=101.000000000", "lob standard snapshot instrument 1 best bid");
    requireContains(text, "best_ask=105.000000000", "lob standard snapshot instrument 1 best ask");
    requireContains(text, "instrument_id=2", "lob standard snapshot includes instrument 2");
    requireContains(text, "best_bid=200.000000000", "lob standard snapshot instrument 2 best bid");
    requireContains(text, "best_ask=<none>", "lob standard snapshot instrument 2 empty ask");

    std::filesystem::remove_all(dir);
}

void testStandardRunnerReportsZeroChronologicalViolationsForSortedFile() {
    const auto dir = makeTempDir("standard_lob_chronological");
    const auto file = writeSyntheticLobFile(dir);

    std::ostringstream out;
    std::ostringstream err;
    LobMarketDataEventProcessor processor{out};
    const auto result = StandardRunner{}.run(file, processor, false, err);

    require(result.summary.chronological_violations == 0, "sorted lob synthetic file has no chronological violations");

    std::filesystem::remove_all(dir);
}

void testLobStandardFinalOutputContainsRequiredSections() {
    const auto dir = makeTempDir("standard_lob_final_output");
    const auto file = writeSyntheticLobFile(dir);

    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.snapshot_interval_events = 4;
    config.max_snapshots = 2;
    config.snapshot_depth = 2;
    LobMarketDataEventProcessor processor{out, config};

    const auto result = StandardRunner{}.run(file, processor, false, err);
    processor.printFinalSummary(out);
    printRunResult(result, out, false, 0);

    const auto text = out.str();
    require(occurrenceCount(text, "LOB Snapshot") == 2, "final output snapshot count");
    requireContains(text, "LOB Snapshot", "final output contains snapshots");
    requireContains(text, "Final LOB Summary", "final output contains final lob summary");
    requireContains(text, "Summary", "final output contains run summary");
    requireContains(text, "instrument_count=2", "final output instrument count");
    requireContains(text, "processed_events=8", "final output lob processed events");
    requireContains(text, "unresolved_events=0", "final output unresolved events");
    requireContains(text, "total_messages_processed=8", "final output total events");
    requireContains(text, "chronological_violations=0", "final output chronological violations");
    requireContains(text, "first_timestamp=100", "final output first timestamp");
    requireContains(text, "last_timestamp=107", "final output last timestamp");
    require(lineValueAfter(text, "wall_clock_seconds=") > 0.0, "final output wall clock is positive");
    require(
        lineValueAfter(text, "throughput_messages_per_second=") > 0.0,
        "final output throughput is positive"
    );

    requireContains(text, "instrument_id=1", "final output instrument 1");
    requireContains(text, "resting_orders=3", "final output instrument 1 resting orders");
    requireContains(text, "best_bid=101.000000000", "final output instrument 1 best bid");
    requireContains(text, "best_ask=105.000000000", "final output instrument 1 best ask");
    requireContains(text, "instrument_id=2", "final output instrument 2");
    requireContains(text, "resting_orders=1", "final output instrument 2 resting orders");
    requireContains(text, "best_bid=200.000000000", "final output instrument 2 best bid");
    requireContains(text, "best_ask=<none>", "final output instrument 2 empty ask");

    std::filesystem::remove_all(dir);
}

} // namespace md::test
