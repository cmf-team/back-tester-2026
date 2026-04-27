// Civil-date <-> days-since-epoch arithmetic (Howard Hinnant, public algorithm).
// Header-only so every translation unit inlines the math in hot loops.

#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>

namespace cmf {

// Days from 1970-01-01 to (y, m, d) in the proleptic Gregorian calendar.
// Works for any year in `int` range. Leap-second-agnostic (UTC ns inputs).
constexpr int64_t daysFromCivil(int y, unsigned m, unsigned d) noexcept {
  y -= m <= 2;
  const int      era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * (m > 2 ? m - 3 : m + 9) + 2u) / 5u + d - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return static_cast<int64_t>(era) * 146097LL
       + static_cast<int64_t>(doe) - 719468LL;
}

struct Ymd {
  int      year;
  unsigned month;   // 1..12
  unsigned day;     // 1..31
};

// Inverse of daysFromCivil.
constexpr Ymd civilFromDays(int64_t z) noexcept {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
  const int y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
  const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
  const unsigned mp  = (5u * doy + 2u) / 153u;
  const unsigned d   = doy - (153u * mp + 2u) / 5u + 1u;
  const unsigned m   = mp < 10u ? mp + 3u : mp - 9u;
  return Ymd{y + static_cast<int>(m <= 2), m, d};
}

// Parses "YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ" (30 chars) into ns since epoch.
inline NanoTime parseIsoTs(const char* p) noexcept {
  auto d2 = [](const char* q) noexcept {
    return unsigned(q[0] - '0') * 10u + unsigned(q[1] - '0');
  };
  const int      year   = int(unsigned(p[0] - '0') * 1000u
                             + unsigned(p[1] - '0') * 100u
                             + unsigned(p[2] - '0') * 10u
                             + unsigned(p[3] - '0'));
  const unsigned month  = d2(p + 5);
  const unsigned day    = d2(p + 8);
  const unsigned hour   = d2(p + 11);
  const unsigned minute = d2(p + 14);
  const unsigned second = d2(p + 17);
  uint32_t nanos = 0;
  for (int i = 0; i < 9; ++i) nanos = nanos * 10u + unsigned(p[20 + i] - '0');

  const int64_t days = daysFromCivil(year, month, day);
  const int64_t secs = days * 86400LL
                     + int64_t(hour) * 3600
                     + int64_t(minute) * 60
                     + int64_t(second);
  return secs * 1'000'000'000LL + int64_t(nanos);
}

} // namespace cmf
