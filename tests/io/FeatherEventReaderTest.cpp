#include "TestSupport.hpp"

#include "io/FeatherEventReader.hpp"
#include "processing/LobMarketDataEventProcessor.hpp"
#include "runners/FlatMergeRunner.hpp"
#include "runners/HierarchicalMergeRunner.hpp"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/feather.h>

#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace md::test {
namespace {

struct FeatherRow {
    std::string ts_recv;
    std::string ts_event;
    std::uint64_t instrument_id{};
    std::string order_id;
    std::string side;
    std::string action;
    std::optional<std::string> price;
    std::uint64_t size{};
};

void requireOk(const arrow::Status& status, const std::string& message) {
    require(status.ok(), message + ": " + status.ToString());
}

template <typename Result>
auto unwrap(Result&& result, const std::string& message) {
    require(result.ok(), message + ": " + result.status().ToString());
    return *std::forward<Result>(result);
}

void appendString(arrow::StringBuilder& builder, const std::string& value) {
    requireOk(builder.Append(value), "append string");
}

void writeFeatherFile(
    const std::filesystem::path& path,
    const std::vector<FeatherRow>& rows,
    bool include_ts_recv = true
) {
    arrow::StringBuilder ts_recv;
    arrow::StringBuilder ts_event;
    arrow::UInt64Builder instrument_id;
    arrow::StringBuilder order_id;
    arrow::StringBuilder side;
    arrow::StringBuilder action;
    arrow::StringBuilder price;
    arrow::UInt64Builder size;

    for (const auto& row : rows) {
        appendString(ts_recv, row.ts_recv);
        appendString(ts_event, row.ts_event);
        requireOk(instrument_id.Append(row.instrument_id), "append instrument_id");
        appendString(order_id, row.order_id);
        appendString(side, row.side);
        appendString(action, row.action);
        if (row.price.has_value()) {
            appendString(price, *row.price);
        } else {
            requireOk(price.AppendNull(), "append null price");
        }
        requireOk(size.Append(row.size), "append size");
    }

    std::shared_ptr<arrow::Array> ts_recv_array;
    std::shared_ptr<arrow::Array> ts_event_array;
    std::shared_ptr<arrow::Array> instrument_id_array;
    std::shared_ptr<arrow::Array> order_id_array;
    std::shared_ptr<arrow::Array> side_array;
    std::shared_ptr<arrow::Array> action_array;
    std::shared_ptr<arrow::Array> price_array;
    std::shared_ptr<arrow::Array> size_array;

    requireOk(ts_recv.Finish(&ts_recv_array), "finish ts_recv");
    requireOk(ts_event.Finish(&ts_event_array), "finish ts_event");
    requireOk(instrument_id.Finish(&instrument_id_array), "finish instrument_id");
    requireOk(order_id.Finish(&order_id_array), "finish order_id");
    requireOk(side.Finish(&side_array), "finish side");
    requireOk(action.Finish(&action_array), "finish action");
    requireOk(price.Finish(&price_array), "finish price");
    requireOk(size.Finish(&size_array), "finish size");

    std::vector<std::shared_ptr<arrow::Field>> fields;
    std::vector<std::shared_ptr<arrow::Array>> arrays;
    if (include_ts_recv) {
        fields.push_back(arrow::field("ts_recv", arrow::utf8()));
        arrays.push_back(ts_recv_array);
    }
    fields.push_back(arrow::field("ts_event", arrow::utf8()));
    fields.push_back(arrow::field("instrument_id", arrow::uint64()));
    fields.push_back(arrow::field("order_id", arrow::utf8()));
    fields.push_back(arrow::field("side", arrow::utf8()));
    fields.push_back(arrow::field("action", arrow::utf8()));
    fields.push_back(arrow::field("price", arrow::utf8()));
    fields.push_back(arrow::field("size", arrow::uint64()));

    arrays.push_back(ts_event_array);
    arrays.push_back(instrument_id_array);
    arrays.push_back(order_id_array);
    arrays.push_back(side_array);
    arrays.push_back(action_array);
    arrays.push_back(price_array);
    arrays.push_back(size_array);

    const auto schema = arrow::schema(fields);
    const auto table = arrow::Table::Make(schema, arrays);

    std::filesystem::create_directories(path.parent_path());
    auto output = unwrap(arrow::io::FileOutputStream::Open(path), "open feather output");
    requireOk(arrow::ipc::feather::WriteTable(*table, output.get()), "write feather file");
    requireOk(output->Close(), "close feather output");
}

std::vector<FeatherRow> file0Rows() {
    return {
        {"100", "100", 1, "1", "B", "A", "100000000000", 10},
        {"400", "400", 1, "1", "", "C", std::nullopt, 4},
    };
}

std::vector<FeatherRow> file1Rows() {
    return {
        {"200", "200", 1, "2", "A", "A", "105000000000", 7},
        {"500", "500", 2, "4", "A", "A", "210000000000", 5},
    };
}

std::vector<FeatherRow> file2Rows() {
    return {
        {"300", "300", 2, "3", "B", "A", "200000000000", 20},
        {"600", "600", 1, "2", "A", "M", "104000000000", 5},
        {"700", "700", 0, "3", "", "C", std::nullopt, 8},
    };
}

std::filesystem::path makeSyntheticFeatherDir() {
    const auto dir = makeTempDir("feather_hard_lob");
    writeFeatherFile(dir / "file_0.feather", file0Rows());
    writeFeatherFile(dir / "file_1.feather", file1Rows());
    writeFeatherFile(dir / "file_2.feather", file2Rows());
    return dir;
}

std::string jsonFlatDigest() {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.max_snapshots = 0;
    LobMarketDataEventProcessor processor{out, config};
    const auto result = FlatMergeRunner{}.run(testDataDir() / "hard_lob_synthetic", processor, false, err);
    require(result.summary.total_messages_processed == 7, "json flat synthetic message count");
    require(result.summary.chronological_violations == 0, "json flat synthetic chronological violations");
    return processor.books().stableStateDigest();
}

std::string jsonHierarchyDigest() {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.max_snapshots = 0;
    LobMarketDataEventProcessor processor{out, config};
    const auto result = HierarchicalMergeRunner{}.run(testDataDir() / "hard_lob_synthetic", processor, false, err);
    require(result.summary.total_messages_processed == 7, "json hierarchy synthetic message count");
    require(result.summary.chronological_violations == 0, "json hierarchy synthetic chronological violations");
    return processor.books().stableStateDigest();
}

std::string featherFlatDigest(const std::filesystem::path& dir) {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.max_snapshots = 0;
    LobMarketDataEventProcessor processor{out, config};
    const auto result = FlatMergeRunner{}.run(dir, processor, false, err, InputFormat::Feather);
    require(result.summary.total_messages_processed == 7, "feather flat synthetic message count");
    require(result.summary.chronological_violations == 0, "feather flat synthetic chronological violations");
    require(result.diagnostics.total_lines_read == 7, "feather flat synthetic rows read");
    require(processor.books().unresolvedEvents() == 0, "feather flat synthetic unresolved events");
    return processor.books().stableStateDigest();
}

std::string featherHierarchyDigest(const std::filesystem::path& dir) {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.max_snapshots = 0;
    LobMarketDataEventProcessor processor{out, config};
    const auto result = HierarchicalMergeRunner{}.run(dir, processor, false, err, InputFormat::Feather);
    require(result.summary.total_messages_processed == 7, "feather hierarchy synthetic message count");
    require(result.summary.chronological_violations == 0, "feather hierarchy synthetic chronological violations");
    require(result.diagnostics.total_lines_read == 7, "feather hierarchy synthetic rows read");
    require(processor.books().unresolvedEvents() == 0, "feather hierarchy synthetic unresolved events");
    return processor.books().stableStateDigest();
}

struct RealLobRun {
    std::size_t messages{};
    std::size_t chronological_violations{};
    std::size_t unresolved_events{};
    std::string digest;
};

RealLobRun runRealLobDigest(
    const std::filesystem::path& dir,
    InputFormat input_format,
    bool hierarchy
) {
    std::ostringstream out;
    std::ostringstream err;
    LobProcessorConfig config;
    config.snapshot_interval_events = 0;
    config.max_snapshots = 0;
    LobMarketDataEventProcessor processor{out, config};

    const auto result = hierarchy
        ? HierarchicalMergeRunner{}.run(dir, processor, false, err, input_format)
        : FlatMergeRunner{}.run(dir, processor, false, err, input_format);
    processor.finishSnapshots();

    return RealLobRun{
        .messages = result.summary.total_messages_processed,
        .chronological_violations = result.summary.chronological_violations,
        .unresolved_events = processor.books().unresolvedEvents(),
        .digest = processor.books().stableStateDigest(),
    };
}

bool realFolderFeatherTestEnabled() {
    const char* enabled = std::getenv("MD_RUN_REAL_FEATHER_TEST");
    return enabled != nullptr && std::string_view{enabled} == "1";
}

std::filesystem::path envOrDefaultPath(const char* env_name, const std::filesystem::path& fallback) {
    const char* value = std::getenv(env_name);
    if (value == nullptr || std::string_view{value}.empty()) {
        return fallback;
    }
    return value;
}

} // namespace

void testFeatherReaderReadsSyntheticRows() {
    const auto dir = makeTempDir("feather_reader_rows");
    const auto path = dir / "sample.feather";
    writeFeatherFile(path, file0Rows());

    std::vector<MarketDataEvent> events;
    FeatherEventReader{path}.readAll(7, [&events](const MarketDataEvent& event) {
        events.push_back(event);
    });

    require(events.size() == 2, "feather reader synthetic row count");
    require(events[0].timestamp == 100, "feather reader first timestamp");
    require(events[0].source_file_id == 7, "feather reader source file id");
    require(events[0].source_sequence == 1, "feather reader source sequence");
    require(events[0].instrument_id == 1, "feather reader instrument id");
    require(events[0].order_id == 1, "feather reader order id");
    require(events[0].side == Side::Bid, "feather reader side");
    require(events[0].action == Action::Add, "feather reader action");
    require(events[0].price == 100000000000LL, "feather reader price");
    require(events[0].size == 10, "feather reader size");

    std::filesystem::remove_all(dir);
}

void testFeatherReaderMapsNullPriceToUndef() {
    const auto dir = makeTempDir("feather_reader_null_price");
    const auto path = dir / "sample.feather";
    writeFeatherFile(path, file0Rows());

    std::vector<MarketDataEvent> events;
    FeatherEventReader{path}.readAll(0, [&events](const MarketDataEvent& event) {
        events.push_back(event);
    });

    require(events.size() == 2, "feather reader null price row count");
    require(
        events[1].price == std::numeric_limits<std::int64_t>::max(),
        "feather reader maps null price to undef"
    );

    std::filesystem::remove_all(dir);
}

void testFeatherReaderFallsBackToTsEventWhenTsRecvColumnIsMissing() {
    const auto dir = makeTempDir("feather_reader_ts_event_fallback");
    const auto path = dir / "sample.feather";
    writeFeatherFile(path, file0Rows(), false);

    std::vector<MarketDataEvent> events;
    FeatherEventReader{path}.readAll(0, [&events](const MarketDataEvent& event) {
        events.push_back(event);
    });

    require(events.size() == 2, "feather reader ts_event fallback row count");
    require(events[0].ts_recv == 0, "feather reader missing ts_recv leaves field zero");
    require(events[0].ts_event == 100, "feather reader reads ts_event");
    require(events[0].timestamp == 100, "feather reader falls back to ts_event");

    std::filesystem::remove_all(dir);
}

void testFeatherFlatLobMatchesJsonFlatLobOnSynthetic() {
    const auto dir = makeSyntheticFeatherDir();
    require(featherFlatDigest(dir) == jsonFlatDigest(), "feather flat digest matches json flat digest");
    std::filesystem::remove_all(dir);
}

void testFeatherHierarchyLobMatchesJsonHierarchyLobOnSynthetic() {
    const auto dir = makeSyntheticFeatherDir();
    require(
        featherHierarchyDigest(dir) == jsonHierarchyDigest(),
        "feather hierarchy digest matches json hierarchy digest"
    );
    std::filesystem::remove_all(dir);
}

void testFeatherFlatAndHierarchyHaveSameDigestOnSynthetic() {
    const auto dir = makeSyntheticFeatherDir();
    require(
        featherFlatDigest(dir) == featherHierarchyDigest(dir),
        "feather flat and hierarchy synthetic digests match"
    );
    std::filesystem::remove_all(dir);
}

void testFeatherFlatAndHierarchyHaveSameDigestOnRealFolder() {
    if (!realFolderFeatherTestEnabled()) {
        return;
    }

    const auto json_dir = envOrDefaultPath(
        "MD_REAL_JSON_FOLDER",
        testDataDir().parent_path().parent_path() / "data" / "XEUR-20260409-HTT6HHLT6R"
    );
    const auto feather_dir = envOrDefaultPath(
        "MD_REAL_FEATHER_FOLDER",
        testDataDir().parent_path().parent_path() / "data_feather" / "XEUR-20260409-HTT6HHLT6R"
    );

    require(std::filesystem::is_directory(json_dir), "real JSON folder exists");
    require(std::filesystem::is_directory(feather_dir), "real Feather folder exists");

    const auto json_flat = runRealLobDigest(json_dir, InputFormat::Json, false);
    const auto feather_flat = runRealLobDigest(feather_dir, InputFormat::Feather, false);
    const auto feather_hierarchy = runRealLobDigest(feather_dir, InputFormat::Feather, true);

    require(json_flat.messages == feather_flat.messages, "real JSON and Feather flat message counts match");
    require(json_flat.digest == feather_flat.digest, "real JSON and Feather flat LOB digests match");
    require(feather_flat.digest == feather_hierarchy.digest, "real Feather flat and hierarchy LOB digests match");
    require(feather_flat.chronological_violations == 0, "real Feather flat chronological violations");
    require(feather_hierarchy.chronological_violations == 0, "real Feather hierarchy chronological violations");
    require(feather_flat.unresolved_events == 0, "real Feather flat unresolved events");
    require(feather_hierarchy.unresolved_events == 0, "real Feather hierarchy unresolved events");
}

} // namespace md::test
