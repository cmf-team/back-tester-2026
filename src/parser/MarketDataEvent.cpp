#include "parser/MarketDataEvent.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <ostream>

namespace cmf {

namespace {

// Days from civil date (Howard Hinnant's algorithm). Constexpr-friendly,
// works for the full range of int years.
constexpr std::int64_t daysFromCivil(int y, unsigned m, unsigned d) noexcept {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153u * (m > 2 ? m - 3 : m + 9) + 2u) / 5u + d - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return static_cast<std::int64_t>(era) * 146097LL +
         static_cast<std::int64_t>(doe) - 719468LL;
}

struct YmdHmsNs {
  int year{};
  unsigned month{}, day{};
  unsigned hour{}, minute{}, second{};
  std::uint32_t nanos{};
  bool ok{false};
};

// Robust ISO-8601 parser:
//   YYYY-MM-DDTHH:MM:SS[.fraction][Z]
// Accepts 0-9 fractional digits (pads with zeros up to 9 for nanoseconds).
YmdHmsNs parseIsoComponents(std::string_view s) noexcept {
  YmdHmsNs out{};
  if (s.size() < 19)
    return out;

  auto twoDigit = [](const char *p, unsigned &dst) noexcept {
    if (!std::isdigit(static_cast<unsigned char>(p[0])) ||
        !std::isdigit(static_cast<unsigned char>(p[1])))
      return false;
    dst = unsigned(p[0] - '0') * 10u + unsigned(p[1] - '0');
    return true;
  };
  auto fourDigit = [](const char *p, int &dst) noexcept {
    for (int i = 0; i < 4; ++i)
      if (!std::isdigit(static_cast<unsigned char>(p[i])))
        return false;
    dst = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 +
          (p[3] - '0');
    return true;
  };

  const char *p = s.data();
  if (!fourDigit(p, out.year))
    return out;
  if (p[4] != '-' || !twoDigit(p + 5, out.month))
    return out;
  if (p[7] != '-' || !twoDigit(p + 8, out.day))
    return out;
  if (p[10] != 'T' || !twoDigit(p + 11, out.hour))
    return out;
  if (p[13] != ':' || !twoDigit(p + 14, out.minute))
    return out;
  if (p[16] != ':' || !twoDigit(p + 17, out.second))
    return out;

  std::size_t i = 19;
  if (i < s.size() && s[i] == '.') {
    ++i;
    std::uint32_t frac = 0;
    int digits = 0;
    while (i < s.size() &&
           std::isdigit(static_cast<unsigned char>(s[i])) && digits < 9) {
      frac = frac * 10u + unsigned(s[i] - '0');
      ++digits;
      ++i;
    }
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
      ++i; // ignore extra precision
    }
    for (int k = digits; k < 9; ++k)
      frac *= 10u;
    out.nanos = frac;
  }
  // tolerate trailing 'Z' or nothing
  out.ok = true;
  return out;
}

} // namespace

NanoTime parseIso8601Nanos(std::string_view ts) noexcept {
  const auto c = parseIsoComponents(ts);
  if (!c.ok)
    return 0;
  const std::int64_t days = daysFromCivil(c.year, c.month, c.day);
  const std::int64_t secs = days * 86'400LL +
                            std::int64_t(c.hour) * 3'600 +
                            std::int64_t(c.minute) * 60 + std::int64_t(c.second);
  return secs * 1'000'000'000LL + std::int64_t(c.nanos);
}

std::string formatIso8601Nanos(NanoTime ns) {
  // Split into seconds and nanoseconds, then back into civil date.
  // Floor-divides correctly even for negative values.
  std::int64_t secs = ns / 1'000'000'000LL;
  std::int64_t nanos = ns - secs * 1'000'000'000LL;
  if (nanos < 0) {
    nanos += 1'000'000'000LL;
    --secs;
  }
  std::int64_t days = secs / 86'400LL;
  std::int64_t tod = secs - days * 86'400LL;
  if (tod < 0) {
    tod += 86'400LL;
    --days;
  }
  // Inverse of daysFromCivil
  std::int64_t z = days + 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe =
      (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
  const int y0 = static_cast<int>(yoe) + static_cast<int>(era) * 400;
  const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
  const unsigned mp = (5u * doy + 2u) / 153u;
  const unsigned d = doy - (153u * mp + 2u) / 5u + 1u;
  const unsigned m = mp < 10u ? mp + 3u : mp - 9u;
  const int y = y0 + (m <= 2 ? 1 : 0);
  const unsigned hh = unsigned(tod / 3600);
  const unsigned mm = unsigned((tod / 60) % 60);
  const unsigned ss = unsigned(tod % 60);

  std::array<char, 40> buf{};
  std::snprintf(buf.data(), buf.size(),
                "%04d-%02u-%02uT%02u:%02u:%02u.%09lldZ", y, m, d, hh, mm, ss,
                static_cast<long long>(nanos));
  return std::string(buf.data());
}

Action parseAction(char c) noexcept {
  switch (c) {
  case 'A':
    return Action::Add;
  case 'M':
    return Action::Modify;
  case 'C':
    return Action::Cancel;
  case 'R':
    return Action::Clear;
  case 'T':
    return Action::Trade;
  case 'F':
    return Action::Fill;
  default:
    return Action::None;
  }
}

MdSide parseMdSide(char c) noexcept {
  switch (c) {
  case 'B':
    return MdSide::Bid;
  case 'A':
    return MdSide::Ask;
  default:
    return MdSide::None;
  }
}

std::ostream &operator<<(std::ostream &os, const MarketDataEvent &ev) {
  os << "ts=" << formatIso8601Nanos(ev.timestamp)
     << " order_id=" << ev.order_id
     << " side=" << static_cast<char>(ev.side);

  if (priceDefined(ev.price))
    os << " price=" << ev.price;
  else
    os << " price=null";

  os << " size=" << ev.size
     << " action=" << static_cast<char>(ev.action);

  if (!ev.symbol.empty())
    os << " symbol=\"" << ev.symbol << "\"";
  return os;
}

} // namespace cmf
