#include "io/FeatherEventReader.hpp"

#include "parsing/JsonParser.hpp"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/ipc/feather.h>

#include <charconv>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace md {
namespace {

constexpr std::int64_t undef_price = std::numeric_limits<std::int64_t>::max();

std::runtime_error arrowError(const std::filesystem::path& path, const arrow::Status& status) {
    return std::runtime_error(path.string() + ": " + status.ToString());
}

template <typename Result>
auto unwrapArrowResult(const std::filesystem::path& path, Result&& result) {
    if (!result.ok()) {
        throw arrowError(path, result.status());
    }
    return *std::forward<Result>(result);
}

std::shared_ptr<arrow::Array> requireColumn(
    const std::shared_ptr<arrow::RecordBatch>& batch,
    const std::string& name
) {
    const int index = batch->schema()->GetFieldIndex(name);
    if (index < 0) {
        throw std::runtime_error("feather file is missing required column: " + name);
    }
    return batch->column(index);
}

std::shared_ptr<arrow::Array> optionalColumn(
    const std::shared_ptr<arrow::RecordBatch>& batch,
    const std::string& name
) {
    const int index = batch->schema()->GetFieldIndex(name);
    if (index < 0) {
        return {};
    }
    return batch->column(index);
}

std::uint64_t parseUnsigned(std::string_view text) {
    std::uint64_t value = 0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

std::string_view stringValue(const std::shared_ptr<arrow::Array>& array, int64_t row) {
    if (array->IsNull(row)) {
        return {};
    }

    switch (array->type_id()) {
        case arrow::Type::STRING:
            return std::static_pointer_cast<arrow::StringArray>(array)->GetView(row);
        case arrow::Type::LARGE_STRING:
            return std::static_pointer_cast<arrow::LargeStringArray>(array)->GetView(row);
        default:
            throw std::runtime_error("expected string-compatible Arrow column");
    }
}

std::uint64_t unsignedValue(const std::shared_ptr<arrow::Array>& array, int64_t row) {
    if (array->IsNull(row)) {
        return 0;
    }

    switch (array->type_id()) {
        case arrow::Type::UINT64:
            return std::static_pointer_cast<arrow::UInt64Array>(array)->Value(row);
        case arrow::Type::INT64:
            return static_cast<std::uint64_t>(
                std::static_pointer_cast<arrow::Int64Array>(array)->Value(row)
            );
        case arrow::Type::STRING:
        case arrow::Type::LARGE_STRING:
            return parseUnsigned(stringValue(array, row));
        default:
            throw std::runtime_error("expected uint64-compatible Arrow column");
    }
}

std::uint64_t timestampValue(const std::shared_ptr<arrow::Array>& array, int64_t row) {
    if (array->IsNull(row)) {
        return 0;
    }

    switch (array->type_id()) {
        case arrow::Type::UINT64:
            return std::static_pointer_cast<arrow::UInt64Array>(array)->Value(row);
        case arrow::Type::INT64:
            return static_cast<std::uint64_t>(
                std::static_pointer_cast<arrow::Int64Array>(array)->Value(row)
            );
        case arrow::Type::STRING:
        case arrow::Type::LARGE_STRING:
            return parseTimestampText(stringValue(array, row));
        default:
            throw std::runtime_error("expected timestamp-compatible Arrow column");
    }
}

std::int64_t priceValue(const std::shared_ptr<arrow::Array>& array, int64_t row) {
    if (array->IsNull(row)) {
        return undef_price;
    }

    switch (array->type_id()) {
        case arrow::Type::INT64:
            return std::static_pointer_cast<arrow::Int64Array>(array)->Value(row);
        case arrow::Type::UINT64:
            return static_cast<std::int64_t>(
                std::static_pointer_cast<arrow::UInt64Array>(array)->Value(row)
            );
        case arrow::Type::STRING:
        case arrow::Type::LARGE_STRING:
            return parsePriceText(stringValue(array, row));
        default:
            throw std::runtime_error("expected price-compatible Arrow column");
    }
}

Side sideValue(const std::shared_ptr<arrow::Array>& array, int64_t row) {
    return parseSideText(stringValue(array, row));
}

Action actionValue(const std::shared_ptr<arrow::Array>& array, int64_t row) {
    return parseActionText(stringValue(array, row));
}

} // namespace

FeatherEventReader::FeatherEventReader(std::filesystem::path file_path)
    : file_path_(std::move(file_path)) {}

void FeatherEventReader::readAll(
    std::uint32_t source_file_id,
    const std::function<void(const MarketDataEvent&)>& on_event
) const {
    auto input = unwrapArrowResult(file_path_, arrow::io::ReadableFile::Open(file_path_));
    auto reader = unwrapArrowResult(file_path_, arrow::ipc::feather::Reader::Open(input));

    std::shared_ptr<arrow::Table> table;
    const arrow::Status read_status = reader->Read(&table);
    if (!read_status.ok()) {
        throw arrowError(file_path_, read_status);
    }

    arrow::TableBatchReader batch_reader(*table);
    std::shared_ptr<arrow::RecordBatch> batch;
    std::uint64_t source_sequence = 0;
    while (true) {
        const arrow::Status next_status = batch_reader.ReadNext(&batch);
        if (!next_status.ok()) {
            throw arrowError(file_path_, next_status);
        }
        if (!batch) {
            break;
        }

        const auto ts_recv = optionalColumn(batch, "ts_recv");
        const auto ts_event = requireColumn(batch, "ts_event");
        const auto instrument_id = requireColumn(batch, "instrument_id");
        const auto order_id = requireColumn(batch, "order_id");
        const auto side = requireColumn(batch, "side");
        const auto action = requireColumn(batch, "action");
        const auto price = requireColumn(batch, "price");
        const auto size = requireColumn(batch, "size");

        for (int64_t row = 0; row < batch->num_rows(); ++row) {
            ++source_sequence;
            MarketDataEvent event{
                .ts_recv = ts_recv ? timestampValue(ts_recv, row) : 0,
                .ts_event = timestampValue(ts_event, row),
                .order_id = unsignedValue(order_id, row),
                .side = sideValue(side, row),
                .price = priceValue(price, row),
                .size = unsignedValue(size, row),
                .action = actionValue(action, row),
                .instrument_id = unsignedValue(instrument_id, row),
                .source_file_id = source_file_id,
                .source_sequence = source_sequence,
                .line_number = static_cast<std::size_t>(source_sequence),
            };
            event.timestamp = event.ts_recv != 0 ? event.ts_recv : event.ts_event;
            on_event(event);
        }
    }
}

} // namespace md
