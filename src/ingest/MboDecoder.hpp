// Per-line JSON → MarketDataEvent decoder for Databento MBO records.
//
// Opaque handle hides simdjson from public headers; one decoder holds a
// long-lived parser so per-line parses reuse its buffers.

#pragma once

#include "common/MarketDataEvent.hpp"

#include <memory>
#include <optional>
#include <string_view>

namespace cmf {

// Why the decoder reports: we need to distinguish "valid JSON but not MBO"
// (skipped_rtype) from "parse failure" (skipped_parse) so the summary matches
// the spec's wording of "every valid message".
enum class DecodeOutcome {
  Ok,
  SkippedRtype,
  ParseError,
};

struct DecodeResult {
  DecodeOutcome outcome;
  MarketDataEvent event; // only meaningful when outcome == Ok
};

class MboDecoder {
public:
  MboDecoder();
  ~MboDecoder();
  MboDecoder(const MboDecoder &) = delete;
  MboDecoder &operator=(const MboDecoder &) = delete;
  MboDecoder(MboDecoder &&) noexcept;
  MboDecoder &operator=(MboDecoder &&) noexcept;

  DecodeResult decodeLine(std::string_view line);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace cmf
