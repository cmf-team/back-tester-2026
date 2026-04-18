#include "data_layer/plain_json_parser/PlainJsonParserUtils.hpp"

#include <cstring>

namespace data_layer::json_line {

void skipWhitespace(const char *&p, const char *const end) {
  while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
    ++p;
  }
}

bool isBlankLine(const std::string_view line) noexcept {
  for (const char c : line) {
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      return false;
    }
  }
  return true;
}

bool skipToKey(const char *&p, const char *const end,
                 const std::string_view key) noexcept {
  const auto remaining = static_cast<std::size_t>(end - p);
  const void *found = memmem(p, remaining, key.data(), key.size());
  if (!found) {
    return false;
  }
  p = static_cast<const char *>(found) + key.size();
  return true;
}

bool readQuotedView(const char *&p, const char *const end,
                      std::string_view &out) noexcept {
  skipWhitespace(p, end);
  if (p >= end || *p != '"') {
    return false;
  }
  ++p;
  const void *quoted_end =
      std::memchr(p, '"', static_cast<std::size_t>(end - p));
  if (!quoted_end) {
    return false;
  }
  const auto *q = static_cast<const char *>(quoted_end);
  out = std::string_view(p, static_cast<std::size_t>(q - p));
  p = q + 1;
  return true;
}

std::optional<std::uint64_t>
parseUtcTimestampNs(const std::string_view timestamp) noexcept {
  if (timestamp.size() != 30 || timestamp.back() != 'Z' ||
      timestamp[4] != '-' || timestamp[7] != '-' || timestamp[10] != 'T' ||
      timestamp[13] != ':' || timestamp[16] != ':' || timestamp[19] != '.') {
    return std::nullopt;
  }

  const char *ptr = timestamp.data();
  const auto readFixed = [ptr](int off, int len) -> std::int64_t {
    std::int64_t value = 0;
    for (int i = 0; i < len; ++i) {
      const char c = ptr[off + i];
      if (c < '0' || c > '9') {
        return -1;
      }
      value = value * 10 + static_cast<std::int64_t>(c - '0');
    }
    return value;
  };

  const std::int64_t y = readFixed(0, 4);
  const std::int64_t mo = readFixed(5, 2);
  const std::int64_t d = readFixed(8, 2);
  const std::int64_t hou = readFixed(11, 2);
  const std::int64_t min = readFixed(14, 2);
  const std::int64_t sec = readFixed(17, 2);
  if (y < 0 || mo < 0 || d < 0 || hou < 0 || min < 0 || sec < 0) {
    return std::nullopt;
  }
  const int year = static_cast<int>(y);
  const unsigned month = static_cast<unsigned>(mo);
  const unsigned day = static_cast<unsigned>(d);
  const int hour = static_cast<int>(hou);
  const int minute = static_cast<int>(min);
  const int second = static_cast<int>(sec);
  if (month > 12 || day > 31 || hour > 23 || minute > 59 || second > 59) {
    return std::nullopt;
  }
  const std::int64_t n0 = readFixed(20, 4);
  const std::int64_t n1 = readFixed(24, 4);
  const char c28 = ptr[28];
  if (n0 < 0 || n1 < 0 || c28 < '0' || c28 > '9') {
    return std::nullopt;
  }
  const std::uint64_t nanos = static_cast<std::uint64_t>(
      n0 * 100000LL + n1 * 10LL + static_cast<std::int64_t>(c28 - '0'));

  constexpr std::uint64_t k_ns_per_sec{1000000000ULL};

  const auto daysFromCivil = [](int y, unsigned m, unsigned d) -> std::int64_t {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m <= 2 ? 9 : -3)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<std::int64_t>(era) * 146097 +
           static_cast<std::int64_t>(doe) - 719468;
  };

  const std::int64_t epoch_days = daysFromCivil(year, month, day);
  const std::uint64_t whole_seconds = static_cast<std::uint64_t>(
      epoch_days * 86400 + hour * 3600 + minute * 60 + second);
  return whole_seconds * k_ns_per_sec + nanos;
}

std::optional<std::int64_t>
parsePrice1e9(const std::string_view price_text) noexcept {
  if (price_text.empty()) {
    return std::nullopt;
  }

  static constexpr std::uint32_t k_pow10[10] = {
      1U,         10U,        100U,       1000U,      10000U,
      100000U,    1000000U,   10000000U,  100000000U, 1000000000U};
  static constexpr std::int64_t k_scale_1e9{1000000000LL};

  bool negative = false;
  std::size_t i = 0;
  if (price_text[0] == '-') {
    negative = true;
    i = 1;
  }

  std::int64_t integer_part = 0;
  for (; i < price_text.size() && price_text[i] != '.'; ++i) {
    const char c = price_text[i];
    if (c < '0' || c > '9') {
      return std::nullopt;
    }
    integer_part = integer_part * 10 + static_cast<std::int64_t>(c - '0');
  }

  std::int64_t fractional_part = 0;
  std::size_t fractional_digits = 0;
  if (i < price_text.size() && price_text[i] == '.') {
    ++i;
    for (; i < price_text.size(); ++i) {
      const char c = price_text[i];
      if (c < '0' || c > '9') {
        return std::nullopt;
      }
      if (fractional_digits < 9) {
        fractional_part =
            fractional_part * 10 + static_cast<std::int64_t>(c - '0');
        ++fractional_digits;
      }
    }
  }
  if (fractional_digits < 9) {
    fractional_part *= k_pow10[9 - fractional_digits];
  }

  const std::int64_t value = integer_part * k_scale_1e9 + fractional_part;
  return negative ? -value : value;
}

} // namespace data_layer::json_line
