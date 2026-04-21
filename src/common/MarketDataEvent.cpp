#include "common/MarketDataEvent.hpp"

#include <chrono>
#include <iomanip>
#include <ostream>
#include <sstream>

namespace cmf {

namespace {

// Format an epoch-ns timestamp as "YYYY-MM-DD HH:MM:SS.nnnnnnnnn" (UTC).
// Using std::chrono so we don't depend on localtime / timezone data.
std::string formatNanoTimeUtc(NanoTime ns) {
  using namespace std::chrono;
  const auto whole_sec = ns / 1'000'000'000LL;
  const auto sub_ns = ns % 1'000'000'000LL;
  const sys_seconds tp{seconds{whole_sec}};
  const auto dp = floor<days>(tp);
  const year_month_day ymd{dp};
  const hh_mm_ss hms{tp - dp};

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << static_cast<int>(ymd.year())
      << '-' << std::setw(2) << static_cast<unsigned>(ymd.month()) << '-'
      << std::setw(2) << static_cast<unsigned>(ymd.day()) << ' ' << std::setw(2)
      << hms.hours().count() << ':' << std::setw(2) << hms.minutes().count()
      << ':' << std::setw(2) << hms.seconds().count() << '.' << std::setw(9)
      << sub_ns;
  return oss.str();
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
