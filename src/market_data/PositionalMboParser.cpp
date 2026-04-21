#include "market_data/PositionalMboParser.hpp"

#include <cstdint>

namespace cmf {

namespace {

// --- primitive scanners (hot path, no bounds checks) ---------------------

// "YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ" (30 chars) -> ns since epoch.
// Unvalidating variant; caller guarantees format and width.
inline NanoTime parseIsoTsFast(const char *p) noexcept {
  auto d2 = [](const char *q) noexcept {
    return static_cast<unsigned>(q[0] - '0') * 10u +
           static_cast<unsigned>(q[1] - '0');
  };
  const int year = static_cast<int>(
      static_cast<unsigned>(p[0] - '0') * 1000u +
      static_cast<unsigned>(p[1] - '0') * 100u +
      static_cast<unsigned>(p[2] - '0') * 10u +
      static_cast<unsigned>(p[3] - '0'));
  const unsigned month = d2(p + 5);
  const unsigned day = d2(p + 8);
  const unsigned hour = d2(p + 11);
  const unsigned minute = d2(p + 14);
  const unsigned second = d2(p + 17);
  std::uint32_t nanos = 0;
  for (int i = 0; i < 9; ++i)
    nanos = nanos * 10u + static_cast<unsigned>(p[20 + i] - '0');

  // Howard Hinnant's days_from_civil.
  int y = year - static_cast<int>(month <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy =
      (153u * (month > 2 ? month - 3 : month + 9) + 2u) / 5u + day - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  const std::int64_t days = static_cast<std::int64_t>(era) * 146097LL +
                            static_cast<std::int64_t>(doe) - 719468LL;

  const std::int64_t secs = days * 86400LL +
                            static_cast<std::int64_t>(hour) * 3600LL +
                            static_cast<std::int64_t>(minute) * 60LL +
                            static_cast<std::int64_t>(second);
  return secs * 1'000'000'000LL + static_cast<std::int64_t>(nanos);
}

// Reads an unquoted unsigned decimal. Returns pointer past the last digit.
template <class T>
inline const char *parseUInt(const char *p, T &out) noexcept {
  std::uint64_t v = 0;
  while (static_cast<unsigned>(*p - '0') < 10u) {
    v = v * 10u + static_cast<std::uint64_t>(*p - '0');
    ++p;
  }
  out = static_cast<T>(v);
  return p;
}

// Reads an unquoted signed decimal (ts_in_delta may be negative).
inline const char *parseInt32(const char *p, std::int32_t &out) noexcept {
  const bool neg = (*p == '-');
  if (neg)
    ++p;
  std::uint32_t v = 0;
  while (static_cast<unsigned>(*p - '0') < 10u) {
    v = v * 10u + static_cast<std::uint32_t>(*p - '0');
    ++p;
  }
  out = neg ? -static_cast<std::int32_t>(v) : static_cast<std::int32_t>(v);
  return p;
}

// Parses a pretty_px decimal price like "1.156100000" (quoted, 9 frac
// digits, optional leading '-'). `p` must point AT the opening '"'.
// Returns cursor past the closing '"'.
inline const char *parsePrice(const char *p, std::int64_t &out) noexcept {
  ++p; // opening quote
  const bool neg = (*p == '-');
  if (neg)
    ++p;
  std::int64_t ipart = 0;
  while (*p != '.') {
    ipart = ipart * 10 + static_cast<std::int64_t>(*p - '0');
    ++p;
  }
  ++p; // '.'
  std::uint32_t frac = 0;
  for (int i = 0; i < 9; ++i)
    frac = frac * 10u + static_cast<unsigned>(p[i] - '0');
  p += 9;
  const std::int64_t px =
      ipart * MarketDataEvent::kPriceScale + static_cast<std::int64_t>(frac);
  out = neg ? -px : px;
  return p + 1; // past closing quote
}

} // namespace

// -------------------------------------------------------------------------

PositionalMboParser::PositionalMboParser(const char *begin,
                                         const char *end) noexcept
    : begin_(begin), cur_(begin), end_(end) {}

void PositionalMboParser::reset(const char *begin, const char *end) noexcept {
  begin_ = begin;
  cur_ = begin;
  end_ = end;
}

bool PositionalMboParser::next(MarketDataEvent &out) {
  if (cur_ >= end_)
    return false;
  const char *p = cur_;

  // {"ts_recv":"
  p += 12;
  out.ts_recv = parseIsoTsFast(p);
  p += 30;

  // ","hd":{"ts_event":"
  p += 20;
  out.ts_event = parseIsoTsFast(p);
  p += 30;

  // ","rtype":
  p += 10;
  p = parseUInt(p, out.rtype);

  // ,"publisher_id":
  p += 16;
  p = parseUInt(p, out.publisher_id);

  // ,"instrument_id":
  p += 17;
  p = parseUInt(p, out.instrument_id);

  // },"action":"
  p += 12;
  out.action = static_cast<MdAction>(*p);
  ++p;

  // ","side":"
  p += 10;
  const char side_c = *p;
  out.side = (side_c == 'B') ? Side::Buy
                             : (side_c == 'A' ? Side::Sell : Side::None);
  ++p;

  // ","price":
  p += 10;
  if (*p == 'n') { // null
    out.price = MarketDataEvent::kUndefPrice;
    p += 4;
  } else {
    p = parsePrice(p, out.price);
  }

  // ,"size":
  p += 8;
  p = parseUInt(p, out.size);

  // ,"channel_id":
  p += 14;
  p = parseUInt(p, out.channel_id);

  // ,"order_id":"
  p += 13;
  p = parseUInt(p, out.order_id);
  // p now points AT the closing '"' of order_id.

  // ","flags":
  p += 10;
  p = parseUInt(p, out.flags);

  // ,"ts_in_delta":
  p += 15;
  p = parseInt32(p, out.ts_in_delta);

  // ,"sequence":
  p += 12;
  p = parseUInt(p, out.sequence);

  // ,"symbol":"
  p += 11;
  const char *sym_begin = p;
  while (*p != '"')
    ++p; // symbols contain spaces/dots but never quotes
  out.symbol.assign(sym_begin, static_cast<std::size_t>(p - sym_begin));

  // closing '"' + '}' + optional '\n'
  p += 2;
  if (p < end_ && *p == '\n')
    ++p;

  cur_ = p;
  return true;
}

} // namespace cmf
