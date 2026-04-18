#include "parser/FileMarketDataSource.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace cmf {

namespace {

// ---------------------------------------------------------------------------
// Tiny key-based JSONL extractor.
//
// The Databento NDJSON we ingest is flat (with one nested "hd" object) and
// keys are unique across the line, so a key-name search is enough to extract
// all the fields we need. This avoids pulling in a full JSON dependency.

// Skips ASCII whitespace.
const char *skipWs(const char *p, const char *end) noexcept {
  while (p != end && (*p == ' ' || *p == '\t'))
    ++p;
  return p;
}

// Locates `"key":` inside [begin,end) and returns a pointer to the first
// non-whitespace character of the value, or nullptr if not found.
const char *findValue(const char *begin, const char *end,
                      std::string_view key) noexcept {
  // Build the search needle: "key":
  std::string needle;
  needle.reserve(key.size() + 4);
  needle.push_back('"');
  needle.append(key.data(), key.size());
  needle.push_back('"');
  needle.push_back(':');

  std::string_view hay(begin, static_cast<std::size_t>(end - begin));
  const std::size_t pos = hay.find(needle);
  if (pos == std::string_view::npos)
    return nullptr;
  return skipWs(begin + pos + needle.size(), end);
}

// Reads a JSON string value that starts at *p == '"'. Returns view into
// the original buffer (no escape handling - vendor data is plain ASCII).
bool readString(const char *&p, const char *end, std::string_view &out) noexcept {
  if (p == end || *p != '"')
    return false;
  const char *first = p + 1;
  const char *q = first;
  while (q != end && *q != '"')
    ++q;
  if (q == end)
    return false;
  out = std::string_view(first, static_cast<std::size_t>(q - first));
  p = q + 1;
  return true;
}

// Reads an unquoted number (int or float) into a string_view spanning it.
bool readNumber(const char *&p, const char *end, std::string_view &out) noexcept {
  const char *first = p;
  if (p != end && (*p == '-' || *p == '+'))
    ++p;
  while (p != end &&
         (std::isdigit(static_cast<unsigned char>(*p)) || *p == '.' ||
          *p == 'e' || *p == 'E' || *p == '+' || *p == '-'))
    ++p;
  if (p == first)
    return false;
  out = std::string_view(first, static_cast<std::size_t>(p - first));
  return true;
}

// Helpers around findValue + read* -----------------------------------------

bool extractString(const char *begin, const char *end, std::string_view key,
                   std::string_view &out) noexcept {
  const char *p = findValue(begin, end, key);
  if (!p)
    return false;
  return readString(p, end, out);
}

template <class IntT>
bool extractInt(const char *begin, const char *end, std::string_view key,
                IntT &out) noexcept {
  const char *p = findValue(begin, end, key);
  if (!p)
    return false;
  // value can be a quoted or unquoted integer ("order_id" comes quoted)
  std::string_view num;
  if (*p == '"') {
    if (!readString(p, end, num))
      return false;
  } else {
    if (!readNumber(p, end, num))
      return false;
  }
  auto [ptr, ec] = std::from_chars(num.data(), num.data() + num.size(), out);
  return ec == std::errc{} && ptr == num.data() + num.size();
}

bool extractDouble(const char *begin, const char *end, std::string_view key,
                   double &out, bool &is_null) noexcept {
  is_null = false;
  const char *p = findValue(begin, end, key);
  if (!p)
    return false;
  // null literal
  if (end - p >= 4 && std::string_view(p, 4) == "null") {
    is_null = true;
    return true;
  }
  // number can be either quoted (pretty_px enables that) or raw.
  std::string_view num;
  if (*p == '"') {
    if (!readString(p, end, num))
      return false;
  } else {
    if (!readNumber(p, end, num))
      return false;
  }
  // std::from_chars for double is supported by libc++ 20+ / libstdc++ 11+, but
  // strtod is a safe fallback that works on every platform.
  std::string tmp(num);
  char *endp = nullptr;
  out = std::strtod(tmp.c_str(), &endp);
  return endp != tmp.c_str();
}

bool extractChar(const char *begin, const char *end, std::string_view key,
                 char &out) noexcept {
  std::string_view sv;
  if (!extractString(begin, end, key, sv) || sv.empty())
    return false;
  out = sv.front();
  return true;
}

} // namespace

ParseStatus parseMarketDataLine(std::string_view line, MarketDataEvent &out) {
  // Trim leading whitespace.
  while (!line.empty() &&
         (line.front() == ' ' || line.front() == '\t' || line.front() == '\r'))
    line.remove_prefix(1);
  while (!line.empty() &&
         (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
    line.remove_suffix(1);

  if (line.empty())
    return ParseStatus::Empty;
  if (line.front() != '{')
    return ParseStatus::Malformed;

  const char *begin = line.data();
  const char *end = begin + line.size();

  out = MarketDataEvent{}; // reset to defaults

  // Required fields ---------------------------------------------------------
  std::string_view ts_event_sv;
  if (!extractString(begin, end, "ts_event", ts_event_sv))
    return ParseStatus::Malformed;
  out.timestamp = parseIso8601Nanos(ts_event_sv);
  if (out.timestamp == 0)
    return ParseStatus::Malformed;

  if (!extractInt(begin, end, "order_id", out.order_id))
    return ParseStatus::Malformed;

  char side_c = 'N';
  if (!extractChar(begin, end, "side", side_c))
    return ParseStatus::Malformed;
  out.side = parseMdSide(side_c);

  bool price_is_null = false;
  double price_d = 0.0;
  if (!extractDouble(begin, end, "price", price_d, price_is_null))
    return ParseStatus::Malformed;
  out.price = price_is_null ? kUndefinedPrice : price_d;

  std::uint64_t size_u = 0;
  if (!extractInt(begin, end, "size", size_u))
    return ParseStatus::Malformed;
  out.size = static_cast<Quantity>(size_u);

  char action_c = 'N';
  if (!extractChar(begin, end, "action", action_c))
    return ParseStatus::Malformed;
  out.action = parseAction(action_c);

  // Auxiliary fields (best effort - missing ones leave defaults) ------------
  std::string_view ts_recv_sv;
  if (extractString(begin, end, "ts_recv", ts_recv_sv))
    out.ts_recv = parseIso8601Nanos(ts_recv_sv);

  extractInt(begin, end, "instrument_id", out.instrument_id);
  extractInt(begin, end, "publisher_id", out.publisher_id);
  extractInt(begin, end, "channel_id", out.channel_id);
  extractInt(begin, end, "sequence", out.sequence);
  extractInt(begin, end, "ts_in_delta", out.ts_in_delta);
  extractInt(begin, end, "rtype", out.rtype);
  extractInt(begin, end, "flags", out.flags);

  std::string_view symbol_sv;
  if (extractString(begin, end, "symbol", symbol_sv))
    out.symbol.assign(symbol_sv.data(), symbol_sv.size());

  return ParseStatus::Ok;
}

// ---------------------------------------------------------------------------
// FileMarketDataSource

FileMarketDataSource::FileMarketDataSource(const std::filesystem::path &path)
    : path_(path), stream_(path) {
  if (!stream_)
    throw std::runtime_error("FileMarketDataSource: cannot open " +
                             path.string());
  // Bigger buffer => fewer syscalls for big files.
  line_buf_.reserve(1024);
}

bool FileMarketDataSource::next(MarketDataEvent &out) {
  while (std::getline(stream_, line_buf_)) {
    ++line_no_;
    switch (parseMarketDataLine(line_buf_, out)) {
    case ParseStatus::Ok:
      return true;
    case ParseStatus::Empty:
      ++skipped_;
      continue;
    case ParseStatus::Malformed:
      ++malformed_;
      continue;
    }
  }
  return false;
}

} // namespace cmf
