#include "FeatherDataParser.hpp"

#include "arrow/array.h"
#include "arrow/io/file.h"
#include "arrow/ipc/feather.h"
#include "arrow/record_batch.h"
#include "arrow/table.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

namespace cmf {

void FeatherDataParser::parse_inner(
    const std::function<void(const MarketDataEvent &)> &f) const {
  if (!path_.string().ends_with(".mbo.json.feather"))
    throw std::runtime_error("FeatherDataParser: unsupported format, expected "
                             ".mbo.json.feather file, got " +
                             path_.string());

  auto file_result = arrow::io::MemoryMappedFile::Open(
      path_.string(), arrow::io::FileMode::READ);
  if (!file_result.ok())
    throw std::runtime_error("FeatherDataParser: cannot open " +
                             path_.string());

  auto reader_result = arrow::ipc::feather::Reader::Open(*file_result);
  if (!reader_result.ok())
    throw std::runtime_error("FeatherDataParser: cannot read feather file " +
                             path_.string());

  std::shared_ptr<arrow::Table> table;
  auto status = (*reader_result)->Read(&table);
  if (!status.ok())
    throw std::runtime_error(
        "FeatherDataParser: cannot read table from feather file " +
        path_.string());

  // Resolve all column indices once from the schema
  const auto &schema = table->schema();
  const int idx_ts_recv = schema->GetFieldIndex("ts_recv");
  const int idx_ts_event = schema->GetFieldIndex("ts_event");
  const int idx_rtype = schema->GetFieldIndex("rtype");
  const int idx_publisher_id = schema->GetFieldIndex("publisher_id");
  const int idx_inst_id = schema->GetFieldIndex("instrument_id");
  const int idx_action = schema->GetFieldIndex("action");
  const int idx_side = schema->GetFieldIndex("side");
  const int idx_price = schema->GetFieldIndex("price");
  const int idx_size = schema->GetFieldIndex("size");
  const int idx_channel_id = schema->GetFieldIndex("channel_id");
  const int idx_order_id = schema->GetFieldIndex("order_id");
  const int idx_flags = schema->GetFieldIndex("flags");
  const int idx_ts_in_delta = schema->GetFieldIndex("ts_in_delta");
  const int idx_sequence = schema->GetFieldIndex("sequence");

  arrow::TableBatchReader reader(*table);
  std::shared_ptr<arrow::RecordBatch> batch;
  while (reader.ReadNext(&batch).ok() && batch) {
    // Cast all columns once per batch
    const auto ts_recv_col =
        std::static_pointer_cast<arrow::Int64Array>(batch->column(idx_ts_recv));
    const auto ts_event_col = std::static_pointer_cast<arrow::Int64Array>(
        batch->column(idx_ts_event));
    const auto rtype_col =
        std::static_pointer_cast<arrow::Int64Array>(batch->column(idx_rtype));
    const auto publisher_id_col = std::static_pointer_cast<arrow::Int64Array>(
        batch->column(idx_publisher_id));
    const auto inst_id_col =
        std::static_pointer_cast<arrow::Int64Array>(batch->column(idx_inst_id));
    const auto action_col = std::static_pointer_cast<arrow::LargeStringArray>(
        batch->column(idx_action));
    const auto side_col = std::static_pointer_cast<arrow::LargeStringArray>(
        batch->column(idx_side));
    const auto price_col =
        std::static_pointer_cast<arrow::DoubleArray>(batch->column(idx_price));
    const auto size_col =
        std::static_pointer_cast<arrow::Int64Array>(batch->column(idx_size));
    const auto channel_id_col = std::static_pointer_cast<arrow::Int64Array>(
        batch->column(idx_channel_id));
    const auto order_id_col = std::static_pointer_cast<arrow::DoubleArray>(
        batch->column(idx_order_id));
    const auto flags_col =
        std::static_pointer_cast<arrow::Int64Array>(batch->column(idx_flags));
    const auto ts_in_delta_col = std::static_pointer_cast<arrow::Int64Array>(
        batch->column(idx_ts_in_delta));
    const auto sequence_col = std::static_pointer_cast<arrow::Int64Array>(
        batch->column(idx_sequence));

    for (int64_t row = 0; row < batch->num_rows(); ++row) {
      try {
        MarketDataEvent e;

        if (!ts_recv_col->IsNull(row))
          e.ts_recv = static_cast<NanoTime>(ts_recv_col->Value(row));

        if (!ts_event_col->IsNull(row))
          e.ts_event = static_cast<NanoTime>(ts_event_col->Value(row));

        if (!rtype_col->IsNull(row))
          e.rtype = static_cast<RType>(rtype_col->Value(row));

        if (!publisher_id_col->IsNull(row))
          e.publisher_id = static_cast<uint32_t>(publisher_id_col->Value(row));

        if (!inst_id_col->IsNull(row))
          e.instrument_id = static_cast<uint32_t>(inst_id_col->Value(row));

        if (!action_col->IsNull(row)) {
          auto s = action_col->GetString(row);
          if (!s.empty())
            e.action = static_cast<Action>(s[0]);
        }

        if (!side_col->IsNull(row)) {
          auto s = side_col->GetString(row);
          if (!s.empty()) {
            const char side_char = s[0];
            switch (side_char) {
            case 'B':
              e.side = Side::Buy;
              break;
            case 'S':
            case 'A':
              e.side = Side::Sell;
              break;
            default:
              e.side = Side::None;
              break;
            }
          }
        }

        if (!price_col->IsNull(row)) {
          double price_val = price_col->Value(row);
          if (!std::isnan(price_val))
            e.price = static_cast<int64_t>(std::round(price_val * 1e9));
        }

        if (!size_col->IsNull(row))
          e.size = static_cast<uint32_t>(size_col->Value(row));

        if (!channel_id_col->IsNull(row))
          e.channel_id = static_cast<uint16_t>(channel_id_col->Value(row));

        if (!order_id_col->IsNull(row))
          e.order_id = static_cast<uint64_t>(order_id_col->Value(row));

        if (!flags_col->IsNull(row))
          e.flags = static_cast<Flags>(flags_col->Value(row));

        if (!ts_in_delta_col->IsNull(row))
          e.ts_in_delta = static_cast<int32_t>(ts_in_delta_col->Value(row));

        if (!sequence_col->IsNull(row))
          e.sequence = static_cast<uint32_t>(sequence_col->Value(row));

        f(e);
      } catch (const std::exception &ex) {
        std::fprintf(stderr, "FeatherDataParser: skipping row: %s\n",
                     ex.what());
      }
    }
  }
}

} // namespace cmf
