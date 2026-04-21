#include "common/MarketDataEvent.hpp"

#include <array>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <ostream>
#include <string>

namespace cmf {

namespace {

// Format an epoch-ns timestamp as "YYYY-MM-DD HH:MM:SS.nnnnnnnnn" (UTC).
// Uses POSIX gmtime_r instead of the C++20 <chrono> calendar types so we
// can build with older libstdc++ (system GCC 10 on Ubuntu 20.04).
std::string formatNanoTimeUtc(NanoTime ns) {
  // Epoch nanoseconds for backtests are always positive, so the sub-second
  // remainder fits in an unsigned 0..999'999'999 range - advertise that to
  // the compiler via unsigned long to keep -Wformat-truncation happy.
  const std::time_t whole_sec = static_cast<std::time_t>(ns / 1'000'000'000LL);
  const unsigned long sub_ns =
      static_cast<unsigned long>(ns % 1'000'000'000LL);

  std::tm tmv{};
  gmtime_r(&whole_sec, &tmv);

  std::array<char, 64> buf{};
  std::snprintf(buf.data(), buf.size(),
                "%04d-%02d-%02d %02d:%02d:%02d.%09lu",
                tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                tmv.tm_hour, tmv.tm_min, tmv.tm_sec, sub_ns);
  return std::string(buf.data());
}

char sideChar(Side s) {
  switch (s) {
  case Side::Buy:
    return 'B';
  case Side::Sell:
    return 'A';
  case Side::None:
  default:
    return 'N';
  }
}

} // namespace

std::ostream &operator<<(std::ostream &os, const MarketDataEvent &ev) {
  const auto prev_flags = os.flags();
  const auto prev_prec = os.precision();

  os << "MDE{ts=" << formatNanoTimeUtc(ev.ts_event)
     << " order_id=" << ev.order_id
     << " side=" << sideChar(ev.side)
     << " price=" << std::fixed << std::setprecision(8) << ev.price
     << " size=" << ev.size
     << " action=" << static_cast<char>(ev.action)
     << " symbol=" << ev.symbolView() << "}";

  os.flags(prev_flags);
  os.precision(prev_prec);
  return os;
}

} // namespace cmf
