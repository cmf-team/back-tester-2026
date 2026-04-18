#include "common/MarketDataEvent.hpp"

#include <cstdint>
#include <cstdio>
#include <ostream>

namespace cmf {

namespace {

// Render int64 fixed-precision (1e-9) price as decimal with 9 dp, preserving
// the sentinel. Avoids std::ostream precision fiddling and handles negatives
// (calendar spreads) correctly without sign-magnitude hazards.
void writePrice(std::ostream &os, std::int64_t price) {
  if (price == UNDEF_PRICE) {
    os << "null";
    return;
  }
  const bool negative = price < 0;
  // Use unsigned arithmetic to safely negate INT64_MIN (not expected, but
  // cheap to defend).
  std::uint64_t mag = negative ? static_cast<std::uint64_t>(-(price + 1)) + 1
                               : static_cast<std::uint64_t>(price);
  const std::uint64_t whole = mag / 1'000'000'000ULL;
  const std::uint64_t frac = mag % 1'000'000'000ULL;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%s%llu.%09llu", negative ? "-" : "",
                static_cast<unsigned long long>(whole),
                static_cast<unsigned long long>(frac));
  os << buf;
}

} // namespace

std::ostream &operator<<(std::ostream &os, const MarketDataEvent &e) {
  os << "ts_recv=" << e.ts_recv << " order_id=" << e.order_id
     << " side=" << static_cast<char>(e.side)
     << " action=" << static_cast<char>(e.action) << " price=";
  writePrice(os, e.price);
  os << " size=" << e.size;
  return os;
}

} // namespace cmf
