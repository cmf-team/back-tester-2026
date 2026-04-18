#include "ingest/MboDecoder.hpp"

#include <simdjson.h>

#include <cctype>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>

namespace cmf {

namespace {

// Databento JSON sometimes emits 64-bit integers as strings to avoid double
// rounding in downstream parsers. Accept both shapes uniformly.

template <class Int> bool parseIntFromString(std::string_view s, Int &out) {
  const char *first = s.data();
  const char *last = first + s.size();
  if (first != last && *first == '+') {
    ++first;
  }
  auto [p, ec] = std::from_chars(first, last, out);
  return ec == std::errc() && p == last;
}

bool readU64(simdjson::dom::element elem, std::uint64_t &out) {
  if (elem.is_uint64()) {
    out = elem.get_uint64().value();
    return true;
  }
  if (elem.is_int64()) {
    std::int64_t v = elem.get_int64().value();
    if (v < 0) {
      return false;
    }
    out = static_cast<std::uint64_t>(v);
    return true;
  }
  if (elem.is_string()) {
    return parseIntFromString(elem.get_string().value(), out);
  }
  return false;
}

bool readI64(simdjson::dom::element elem, std::int64_t &out) {
  if (elem.is_int64()) {
    out = elem.get_int64().value();
    return true;
  }
  if (elem.is_uint64()) {
    std::uint64_t v = elem.get_uint64().value();
    if (v >
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      return false;
    }
    out = static_cast<std::int64_t>(v);
    return true;
  }
  if (elem.is_string()) {
    return parseIntFromString(elem.get_string().value(), out);
  }
  return false;
}

template <class UInt> bool assignUnsigned(std::uint64_t value, UInt &out) {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<UInt>::max())) {
    return false;
  }
  out = static_cast<UInt>(value);
  return true;
}

template <class Int> bool assignSigned(std::int64_t value, Int &out) {
  if (value < static_cast<std::int64_t>(std::numeric_limits<Int>::min()) ||
      value > static_cast<std::int64_t>(std::numeric_limits<Int>::max())) {
    return false;
  }
  out = static_cast<Int>(value);
  return true;
}

constexpr std::uint64_t kNanosPerSecond = 1'000'000'000ULL;

bool parseDigits(std::string_view s, std::size_t pos, std::size_t count,
                 unsigned &out) {
  if (pos + count > s.size()) {
    return false;
  }
  unsigned value = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const unsigned char ch = static_cast<unsigned char>(s[pos + i]);
    if (!std::isdigit(ch)) {
      return false;
    }
    value = value * 10U + static_cast<unsigned>(ch - '0');
  }
  out = value;
  return true;
}

constexpr std::int64_t daysFromCivil(int year, unsigned month,
                                     unsigned day) noexcept {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned shiftedMonth = month > 2 ? month - 3 : month + 9;
  const unsigned doy = (153 * shiftedMonth + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int>(doe) - 719468;
}

bool parseIso8601Timestamp(std::string_view s, std::uint64_t &out) {
  if (s.size() < 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' ||
      s[13] != ':' || s[16] != ':' || s.back() != 'Z') {
    return false;
  }

  unsigned year = 0;
  unsigned month = 0;
  unsigned day = 0;
  unsigned hour = 0;
  unsigned minute = 0;
  unsigned second = 0;
  if (!parseDigits(s, 0, 4, year) || !parseDigits(s, 5, 2, month) ||
      !parseDigits(s, 8, 2, day) || !parseDigits(s, 11, 2, hour) ||
      !parseDigits(s, 14, 2, minute) || !parseDigits(s, 17, 2, second)) {
    return false;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
      minute > 59 || second > 59) {
    return false;
  }

  std::uint64_t nanos = 0;
  if (s.size() > 20) {
    if (s[19] != '.') {
      return false;
    }
    std::size_t pos = 20;
    std::size_t digits = 0;
    while (pos < s.size() - 1) {
      const unsigned char ch = static_cast<unsigned char>(s[pos]);
      if (!std::isdigit(ch) || digits == 9) {
        return false;
      }
      nanos = nanos * 10ULL + static_cast<std::uint64_t>(ch - '0');
      ++pos;
      ++digits;
    }
    if (digits == 0) {
      return false;
    }
    while (digits < 9) {
      nanos *= 10ULL;
      ++digits;
    }
  }

  const std::int64_t days = daysFromCivil(static_cast<int>(year), month, day);
  if (days < 0) {
    return false;
  }
  const std::uint64_t seconds = static_cast<std::uint64_t>(days) * 86400ULL +
                                static_cast<std::uint64_t>(hour) * 3600ULL +
                                static_cast<std::uint64_t>(minute) * 60ULL +
                                static_cast<std::uint64_t>(second);
  out = seconds * kNanosPerSecond + nanos;
  return true;
}

bool readTimestamp(simdjson::dom::element elem, std::uint64_t &out) {
  if (readU64(elem, out)) {
    return true;
  }
  if (!elem.is_string()) {
    return false;
  }
  const std::string_view s = elem.get_string().value();
  return parseIso8601Timestamp(s, out);
}

bool parseFixedPointPrice(std::string_view s, std::int64_t &out) {
  if (s.empty()) {
    return false;
  }

  bool negative = false;
  std::size_t pos = 0;
  if (s[pos] == '+' || s[pos] == '-') {
    negative = s[pos] == '-';
    ++pos;
  }
  if (pos == s.size()) {
    return false;
  }

  std::uint64_t whole = 0;
  std::size_t wholeDigits = 0;
  while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
    const auto digit = static_cast<std::uint64_t>(s[pos] - '0');
    if (whole > (std::numeric_limits<std::uint64_t>::max() - digit) / 10ULL) {
      return false;
    }
    whole = whole * 10ULL + digit;
    ++pos;
    ++wholeDigits;
  }
  if (wholeDigits == 0) {
    return false;
  }

  std::uint64_t frac = 0;
  std::size_t fracDigits = 0;
  if (pos < s.size()) {
    if (s[pos] != '.') {
      return false;
    }
    ++pos;
    while (pos < s.size()) {
      const unsigned char ch = static_cast<unsigned char>(s[pos]);
      if (!std::isdigit(ch) || fracDigits == 9) {
        return false;
      }
      frac = frac * 10ULL + static_cast<std::uint64_t>(ch - '0');
      ++pos;
      ++fracDigits;
    }
    while (fracDigits < 9) {
      frac *= 10ULL;
      ++fracDigits;
    }
  }

  constexpr std::uint64_t kPositiveLimit =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  constexpr std::uint64_t kNegativeLimit = kPositiveLimit + 1ULL;
  const std::uint64_t limit = negative ? kNegativeLimit : kPositiveLimit;
  if (whole > limit / kNanosPerSecond) {
    return false;
  }
  std::uint64_t scaled = whole * kNanosPerSecond;
  if (scaled > limit - frac) {
    return false;
  }
  scaled += frac;

  if (negative) {
    if (scaled == kNegativeLimit) {
      out = std::numeric_limits<std::int64_t>::min();
    } else {
      out = -static_cast<std::int64_t>(scaled);
    }
  } else {
    out = static_cast<std::int64_t>(scaled);
  }
  return true;
}

bool readPrice(simdjson::dom::element elem, std::int64_t &out) {
  if (elem.is_null()) {
    out = UNDEF_PRICE;
    return true;
  }
  if (readI64(elem, out)) {
    return true;
  }
  if (!elem.is_string()) {
    return false;
  }
  const std::string_view s = elem.get_string().value();
  if (s.find('.') != std::string_view::npos) {
    return parseFixedPointPrice(s, out);
  }
  return parseIntFromString(s, out);
}

bool fieldTimestamp(simdjson::dom::object obj, std::string_view key,
                    std::uint64_t &out) {
  auto e = obj[key];
  if (e.error() != simdjson::SUCCESS) {
    return false;
  }
  return readTimestamp(e.value(), out);
}

bool fieldPrice(simdjson::dom::object obj, std::string_view key,
                std::int64_t &out) {
  auto e = obj[key];
  if (e.error() != simdjson::SUCCESS) {
    return false;
  }
  return readPrice(e.value(), out);
}

template <class UInt>
bool fieldUnsignedOpt(simdjson::dom::object obj, std::string_view key,
                      UInt &out) {
  auto e = obj[key];
  if (e.error() != simdjson::SUCCESS) {
    return true;
  }
  std::uint64_t value = 0;
  if (!readU64(e.value(), value)) {
    return false;
  }
  return assignUnsigned(value, out);
}

bool isMboRtype(simdjson::dom::element elem) {
  std::uint64_t n = 0;
  if (readU64(elem, n)) {
    return n == RTYPE_MBO;
  }
  if (elem.is_string()) {
    std::string_view s = elem.get_string().value();
    return s == "mbo";
  }
  return false;
}

char readCharCode(simdjson::dom::object obj, std::string_view key,
                  char fallback) {
  auto e = obj[key];
  if (e.error() != simdjson::SUCCESS) {
    return fallback;
  }
  if (!e.value().is_string()) {
    return fallback;
  }
  std::string_view s = e.value().get_string().value();
  return s.empty() ? fallback : s.front();
}

MdAction toAction(char c) {
  switch (c) {
  case 'A':
    return MdAction::Add;
  case 'M':
    return MdAction::Modify;
  case 'C':
    return MdAction::Cancel;
  case 'R':
    return MdAction::Clear;
  case 'T':
    return MdAction::Trade;
  case 'F':
    return MdAction::Fill;
  default:
    return MdAction::None;
  }
}

MdSide toSide(char c) {
  switch (c) {
  case 'A':
    return MdSide::Ask;
  case 'B':
    return MdSide::Bid;
  default:
    return MdSide::None;
  }
}

} // namespace

struct MboDecoder::Impl {
  simdjson::dom::parser parser;
};

MboDecoder::MboDecoder() : impl_{std::make_unique<Impl>()} {}
MboDecoder::~MboDecoder() = default;
MboDecoder::MboDecoder(MboDecoder &&) noexcept = default;
MboDecoder &MboDecoder::operator=(MboDecoder &&) noexcept = default;

DecodeResult MboDecoder::decodeLine(std::string_view line) {
  DecodeResult result{DecodeOutcome::ParseError, {}};

  auto docResult = impl_->parser.parse(line.data(), line.size());
  if (docResult.error() != simdjson::SUCCESS) {
    return result;
  }
  auto objResult = docResult.get_object();
  if (objResult.error() != simdjson::SUCCESS) {
    return result;
  }
  simdjson::dom::object obj = objResult.value();

  simdjson::dom::object header = obj;
  if (auto hd = obj["hd"]; hd.error() == simdjson::SUCCESS) {
    auto hdObj = hd.value().get_object();
    if (hdObj.error() != simdjson::SUCCESS) {
      return result;
    }
    header = hdObj.value();
  }

  auto rtypeElem = header["rtype"];
  if (rtypeElem.error() != simdjson::SUCCESS ||
      !isMboRtype(rtypeElem.value())) {
    result.outcome = DecodeOutcome::SkippedRtype;
    return result;
  }

  MarketDataEvent e{};

  // Required fields; any missing ⇒ parse error.
  if (!fieldTimestamp(obj, "ts_recv", e.ts_recv)) {
    return result;
  }
  if (!fieldTimestamp(header, "ts_event", e.ts_event)) {
    return result;
  }
  if (!fieldPrice(obj, "price", e.price)) {
    return result;
  }

  if (!fieldUnsignedOpt(obj, "order_id", e.order_id) ||
      !fieldUnsignedOpt(obj, "size", e.size) ||
      !fieldUnsignedOpt(obj, "sequence", e.sequence) ||
      !fieldUnsignedOpt(header, "instrument_id", e.instrument_id) ||
      !fieldUnsignedOpt(header, "publisher_id", e.publisher_id) ||
      !fieldUnsignedOpt(obj, "flags", e.flags) ||
      !fieldUnsignedOpt(obj, "channel_id", e.channel_id)) {
    return result;
  }

  std::int64_t delta = 0;
  if (auto d = obj["ts_in_delta"]; d.error() == simdjson::SUCCESS) {
    if (!readI64(d.value(), delta)) {
      return result;
    }
    if (!assignSigned(delta, e.ts_in_delta)) {
      return result;
    }
  }

  e.action = toAction(readCharCode(obj, "action", 'N'));
  e.side = toSide(readCharCode(obj, "side", 'N'));

  result.outcome = DecodeOutcome::Ok;
  result.event = e;
  return result;
}

} // namespace cmf
