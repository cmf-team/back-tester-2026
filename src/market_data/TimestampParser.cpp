#include "market_data/TimestampParser.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace cmf {

namespace {

// Howard Hinnant's days_from_civil: proleptic Gregorian date -> days since
// 1970-01-01. Correct for any year, including pre-epoch.
constexpr std::int64_t daysFromCivil(int y, unsigned m, unsigned d) noexcept {
  y -= static_cast<int>(m <= 2);
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<std::int64_t>(era) * 146097 +
         static_cast<std::int64_t>(doe) - 719468;
}

template <std::size_t N>
bool readDigits(std::string_view s, std::size_t o, unsigned &out) noexcept {
  unsigned v = 0;
  for (std::size_t i = 0; i < N; ++i) {
    const char c = s[o + i];
    if (c < '0' || c > '9')
      return false;
    v = v * 10u + static_cast<unsigned>(c - '0');
  }
  out = v;
  return true;
}

[[noreturn]] void throwMalformed(std::string_view s, const char *reason) {
  throw std::invalid_argument(std::string{"timestamp: "} + reason + ": '" +
                              std::string{s} + "'");
}

} // namespace

NanoTime parseDatabentoTimestamp(std::string_view s) {
  // Expected: YYYY-MM-DDTHH:MM:SS.fffffffffZ
  // Index:    0123456789012345678901234567890
  //                     1111111111222222222
  constexpr std::size_t kLen = 30;
  if (s.size() != kLen)
    throwMalformed(s, "wrong length");
  if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' ||
      s[16] != ':' || s[19] != '.' || s[29] != 'Z')
    throwMalformed(s, "wrong separators");

  unsigned y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0, ns = 0;
  const bool ok =
      readDigits<4>(s, 0, y) && readDigits<2>(s, 5, mo) &&
      readDigits<2>(s, 8, d) && readDigits<2>(s, 11, h) &&
      readDigits<2>(s, 14, mi) && readDigits<2>(s, 17, se) &&
      readDigits<9>(s, 20, ns);
  if (!ok)
    throwMalformed(s, "non-digit in field");

  if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || se > 60)
    throwMalformed(s, "field out of range");

  const std::int64_t days = daysFromCivil(static_cast<int>(y), mo, d);
  const std::int64_t secs = days * 86400LL +
                            static_cast<std::int64_t>(h) * 3600LL +
                            static_cast<std::int64_t>(mi) * 60LL +
                            static_cast<std::int64_t>(se);
  return secs * 1'000'000'000LL + static_cast<std::int64_t>(ns);
}

} // namespace cmf
