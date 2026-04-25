#include "market_data/ArrowFeatherSource.hpp"

#ifdef BACKTESTER_HAS_ARROW

#include "common/BasicTypes.hpp"

#include <arrow/array.h>
#include <arrow/io/file.h>
#include <arrow/ipc/reader.h>
#include <arrow/result.h>
#include <arrow/status.h>
#include <arrow/table.h>
#include <arrow/type.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace cmf {

namespace {

template <typename ArrayT>
const ArrayT &asArray(const arrow::ChunkedArray &c, int chunk_idx) {
  const auto *a = static_cast<const ArrayT *>(c.chunk(chunk_idx).get());
  return *a;
}

[[noreturn]] void throwArrow(const arrow::Status &st, const char *what) {
  throw std::runtime_error(std::string{"ArrowFeatherSource: "} + what + ": " +
                           st.ToString());
}

[[noreturn]] void throwArrow(const std::string &msg) {
  throw std::runtime_error("ArrowFeatherSource: " + msg);
}

} // namespace

struct ArrowFeatherSource::Impl {
  std::shared_ptr<arrow::Table> table;
  // Cached per-chunk pointers (Arrow tables can be chunked). For files
  // produced by our converter the whole file is one chunk, so we keep it
  // simple and walk linearly.
  int chunk_idx{0};
  int64_t row_in_chunk{0};
  int64_t row_global{0};
  int64_t total{0};
};

ArrowFeatherSource::ArrowFeatherSource(const std::filesystem::path &path)
    : impl_(std::make_unique<Impl>()) {
  auto in = arrow::io::ReadableFile::Open(path.string());
  if (!in.ok())
    throwArrow(in.status(), "open");
  auto reader = arrow::ipc::RecordBatchFileReader::Open(*in);
  if (!reader.ok())
    throwArrow(reader.status(), "RecordBatchFileReader::Open");

  const auto n = (*reader)->num_record_batches();
  arrow::RecordBatchVector batches;
  batches.reserve(n);
  for (int i = 0; i < n; ++i) {
    auto b = (*reader)->ReadRecordBatch(i);
    if (!b.ok())
      throwArrow(b.status(), "ReadRecordBatch");
    batches.push_back(*b);
  }
  auto table = arrow::Table::FromRecordBatches(batches);
  if (!table.ok())
    throwArrow(table.status(), "FromRecordBatches");
  impl_->table = *table;
  impl_->total = impl_->table->num_rows();

  // Validate the schema once — early failure is cheaper than per-row casts.
  static const char *kCols[] = {
      "ts_recv",       "ts_event",  "ts_in_delta", "sequence", "instrument_id",
      "publisher_id", "flags",     "rtype",       "channel_id",
      "action",        "side",      "order_id",    "price",    "size",
      "symbol",
  };
  for (const char *c : kCols) {
    if (impl_->table->schema()->GetFieldByName(c) == nullptr)
      throwArrow(std::string{"missing column '"} + c + "'");
  }
}

ArrowFeatherSource::~ArrowFeatherSource() = default;

std::size_t ArrowFeatherSource::total() const noexcept {
  return static_cast<std::size_t>(impl_->total);
}

bool ArrowFeatherSource::pop(MarketDataEvent &out) {
  if (impl_->row_global >= impl_->total)
    return false;

  // Locate the current chunk-local row. All columns are chunked the same
  // way, so cache once and advance together.
  const auto &t = *impl_->table;

  auto col_int64 = [&](const char *name) -> int64_t {
    const auto &col = *t.GetColumnByName(name);
    auto chunk = col.chunk(impl_->chunk_idx);
    if (impl_->row_in_chunk >= chunk->length()) {
      // not reached; we advance the chunk_idx in the loop body
      throwArrow(std::string{"row out of chunk: "} + name);
    }
    if (chunk->type_id() == arrow::Type::INT64)
      return asArray<arrow::Int64Array>(col, impl_->chunk_idx)
          .Value(impl_->row_in_chunk);
    if (chunk->type_id() == arrow::Type::UINT64)
      return static_cast<int64_t>(
          asArray<arrow::UInt64Array>(col, impl_->chunk_idx)
              .Value(impl_->row_in_chunk));
    if (chunk->type_id() == arrow::Type::INT32)
      return asArray<arrow::Int32Array>(col, impl_->chunk_idx)
          .Value(impl_->row_in_chunk);
    if (chunk->type_id() == arrow::Type::UINT32)
      return static_cast<int64_t>(
          asArray<arrow::UInt32Array>(col, impl_->chunk_idx)
              .Value(impl_->row_in_chunk));
    throwArrow(std::string{"unexpected int type for "} + name);
  };
  auto col_uint = [&](const char *name) -> uint64_t {
    return static_cast<uint64_t>(col_int64(name));
  };

  const auto &col_action = *t.GetColumnByName("action");
  const auto action_byte =
      asArray<arrow::UInt8Array>(col_action, impl_->chunk_idx)
          .Value(impl_->row_in_chunk);
  const auto &col_side = *t.GetColumnByName("side");
  const auto side_int =
      asArray<arrow::Int8Array>(col_side, impl_->chunk_idx)
          .Value(impl_->row_in_chunk);
  const auto &col_symbol = *t.GetColumnByName("symbol");
  const auto sym_view =
      asArray<arrow::StringArray>(col_symbol, impl_->chunk_idx)
          .GetView(impl_->row_in_chunk);

  out.ts_recv = col_int64("ts_recv");
  out.ts_event = col_int64("ts_event");
  out.ts_in_delta = static_cast<int32_t>(col_int64("ts_in_delta"));
  out.sequence = static_cast<uint32_t>(col_uint("sequence"));
  out.instrument_id = col_uint("instrument_id");
  out.publisher_id = static_cast<uint16_t>(col_uint("publisher_id"));
  out.flags = static_cast<uint16_t>(col_uint("flags"));
  out.rtype = static_cast<uint8_t>(col_uint("rtype"));
  out.channel_id = static_cast<uint8_t>(col_uint("channel_id"));
  out.action = static_cast<MdAction>(static_cast<char>(action_byte));
  switch (side_int) {
  case 1:
    out.side = Side::Buy;
    break;
  case -1:
    out.side = Side::Sell;
    break;
  default:
    out.side = Side::None;
    break;
  }
  out.order_id = col_uint("order_id");
  out.price = col_int64("price");
  out.size = static_cast<uint32_t>(col_uint("size"));
  out.symbol.assign(sym_view.data(), sym_view.size());

  ++impl_->row_in_chunk;
  ++impl_->row_global;

  // Advance chunk if drained.
  const auto &any_col = *t.column(0);
  if (impl_->chunk_idx < any_col.num_chunks() &&
      impl_->row_in_chunk >= any_col.chunk(impl_->chunk_idx)->length()) {
    ++impl_->chunk_idx;
    impl_->row_in_chunk = 0;
  }
  return true;
}

std::vector<MarketDataEvent>
loadFeatherEvents(const std::filesystem::path &path) {
  ArrowFeatherSource src(path);
  std::vector<MarketDataEvent> out;
  out.reserve(src.total());
  MarketDataEvent e;
  while (src.pop(e))
    out.push_back(e);
  return out;
}

} // namespace cmf

#endif // BACKTESTER_HAS_ARROW
