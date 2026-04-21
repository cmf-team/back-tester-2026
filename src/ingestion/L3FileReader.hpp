// L3FileReader - streaming reader of one daily zstd-compressed L3/MBO file.
//
// The on-disk format is a raw dump of a numpy structured array with dtype:
//
//   TOrderlog = np.dtype([
//       ('ts_event',     'M8[ns]'),   // int64 ns since epoch
//       ('ts_recv',      'M8[ns]'),   // int64 ns since epoch
//       ('action',       'S1'),
//       ('side',         'S1'),
//       ('price',        'f8'),
//       ('size',         'i8'),
//       ('channel_id',   'i4'),
//       ('order_id',     'u8'),
//       ('flags',        'u1'),
//       ('ts_in_delta',  'i4'),
//       ('sequence',     'i4'),
//       ('symbol',       'S45'),
//       ('rtype',        'i4'),
//       ('publisher_id', 'u4'),
//       ('instrument_id','i4'),
//   ])
//
// The struct is packed (no alignment padding), so each record is exactly
// kRecordSize = 112 bytes. Fields are therefore read via memcpy to avoid
// unaligned-access UB.

#pragma once

#include "common/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace cmf {

// Size of one packed TOrderlog record.
inline constexpr std::size_t kRecordSize = 112;

// Byte offsets of each field in the packed record (verified against numpy).
struct L3RecordLayout {
  static constexpr std::size_t ts_event = 0;
  static constexpr std::size_t ts_recv = 8;
  static constexpr std::size_t action = 16;
  static constexpr std::size_t side = 17;
  static constexpr std::size_t price = 18;
  static constexpr std::size_t size = 26;
  static constexpr std::size_t channel_id = 34;
  static constexpr std::size_t order_id = 38;
  static constexpr std::size_t flags = 46;
  static constexpr std::size_t ts_in_delta = 47;
  static constexpr std::size_t sequence = 51;
  static constexpr std::size_t symbol = 55;
  static constexpr std::size_t rtype = 100;
  static constexpr std::size_t publisher_id = 104;
  static constexpr std::size_t instrument_id = 108;
  static_assert(instrument_id + 4 == kRecordSize);
};

// Decode one packed 112-byte record into a MarketDataEvent. The input pointer
// must reference at least kRecordSize bytes.
void decodeRecord(const std::uint8_t *raw, MarketDataEvent &out) noexcept;

using MarketDataEventHandler = std::function<void(const MarketDataEvent &)>;

// Reads the given .zst file end-to-end in a streaming fashion and invokes
// `onEvent` for each parsed message, in chronological order (i.e. the same
// order they appear on disk). Returns the number of messages processed.
// Throws std::runtime_error on I/O or decompression errors.
std::size_t readL3ZstFile(const std::string &path,
                          const MarketDataEventHandler &onEvent);

} // namespace cmf
