// PositionalMboParser — zero-allocation byte-level parser for Databento MBO
// NDJSON records.
//
// Assumes the fixed key order and layout produced by XEUR/EOBI batch
// downloads with `pretty_px=true`, `pretty_ts=true`:
//   {"ts_recv":"...","hd":{"ts_event":"...","rtype":N,"publisher_id":N,
//    "instrument_id":N},"action":"X","side":"X","price":"D.DDDDDDDDD"|null,
//    "size":N,"channel_id":N,"order_id":"N","flags":N,"ts_in_delta":N,
//    "sequence":N,"symbol":"TEXT"}\n
//
// Behaviour on malformed input is undefined — bounded by the end pointer
// only at record boundaries, not mid-record. For trusted vendor data.

#pragma once

#include "market_data/MarketDataEvent.hpp"

#include <cstddef>

namespace cmf {

class PositionalMboParser {
public:
  PositionalMboParser() noexcept = default;

  // Constructs the parser over the half-open byte range [begin, end).
  PositionalMboParser(const char *begin, const char *end) noexcept;

  // (Re-)binds the parser to a new byte range.
  void reset(const char *begin, const char *end) noexcept;

  // Parses the next record into `out`. Returns false at end-of-buffer.
  bool next(MarketDataEvent &out);

  // Number of bytes consumed since the last reset().
  std::size_t bytesConsumed() const noexcept {
    return static_cast<std::size_t>(cur_ - begin_);
  }

  std::size_t totalBytes() const noexcept {
    return static_cast<std::size_t>(end_ - begin_);
  }

private:
  const char *begin_{nullptr};
  const char *cur_{nullptr};
  const char *end_{nullptr};
};

} // namespace cmf
