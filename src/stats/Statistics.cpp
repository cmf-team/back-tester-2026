#include "stats/Statistics.hpp"

#include "common/TimeUtils.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace cmf {

namespace {

// Flag bits from the Databento standards page.
constexpr uint8_t kFlagMaybeBadBook = 1u << 2;   // 0x04
constexpr uint8_t kFlagBadTsRecv    = 1u << 3;   // 0x08

std::string formatIso(NanoTime ns) {
  if (ns == std::numeric_limits<NanoTime>::max()
      || ns == std::numeric_limits<NanoTime>::min()) {
    return "-";
  }
  constexpr int64_t kNsPerDay = 86400LL * 1'000'000'000LL;
  int64_t days = ns / kNsPerDay;
  int64_t rem  = ns - days * kNsPerDay;
  if (rem < 0) { rem += kNsPerDay; --days; }
  const Ymd     ymd = civilFromDays(days);
  const int64_t sod = rem / 1'000'000'000LL;
  const int64_t ns_ = rem % 1'000'000'000LL;
  const int     h   = static_cast<int>(sod / 3600);
  const int     mi  = static_cast<int>((sod / 60) % 60);
  const int     se  = static_cast<int>(sod % 60);
  char buf[40];
  std::snprintf(buf, sizeof(buf),
                "%04d-%02u-%02uT%02d:%02d:%02d.%09lldZ",
                ymd.year, ymd.month, ymd.day, h, mi, se,
                static_cast<long long>(ns_));
  return buf;
}

std::string formatPrice(int64_t px) {
  if (px == MarketDataEvent::kUndefPrice) return "-";
  const bool neg = px < 0;
  const int64_t a = neg ? -px : px;
  const int64_t ip = a / MarketDataEvent::kPriceScale;
  const int64_t fp = a % MarketDataEvent::kPriceScale;
  char buf[48];
  std::snprintf(buf, sizeof(buf), "%s%lld.%09lld",
                neg ? "-" : "",
                static_cast<long long>(ip),
                static_cast<long long>(fp));
  return buf;
}

} // namespace

// ---------------------------------------------------------------------------

void Statistics::onEvent(const MarketDataEvent& ev) noexcept {
  ++total_events_;

  if (ev.ts_recv < first_ts_) first_ts_ = ev.ts_recv;
  if (ev.ts_recv > last_ts_)  last_ts_  = ev.ts_recv;

  ++action_counts_[static_cast<uint8_t>(ev.action)];
  ++publisher_counts_[ev.publisher_id];

  if (ev.flags & kFlagMaybeBadBook) ++maybe_bad_book_events_;
  if (ev.flags & kFlagBadTsRecv)    ++bad_ts_recv_events_;

  auto& s = instrument_stats_[ev.instrument_id];
  ++s.count;
  if (ev.ts_recv < s.first_ts) s.first_ts = ev.ts_recv;
  if (s.last_ts > std::numeric_limits<NanoTime>::min()
      && ev.ts_recv < s.last_ts) {
    ++s.ts_recv_regressions;
    ++ts_recv_regressions_;
  }
  if (ev.ts_recv > s.last_ts) s.last_ts = ev.ts_recv;

  if (!ev.symbol.empty() && s.symbol != ev.symbol) s.symbol = ev.symbol;

  if (ev.priceDefined()) {
    if (ev.price < min_price_) min_price_ = ev.price;
    if (ev.price > max_price_) max_price_ = ev.price;
    if (ev.price < s.min_price) s.min_price = ev.price;
    if (ev.price > s.max_price) s.max_price = ev.price;
  }

  avg_delta_ns_ += (static_cast<double>(ev.ts_in_delta) - avg_delta_ns_) / static_cast<double>(total_events_);
}

void Statistics::reset() noexcept {
  total_events_          = 0;
  first_ts_              = std::numeric_limits<NanoTime>::max();
  last_ts_               = std::numeric_limits<NanoTime>::min();
  action_counts_.fill(0);
  publisher_counts_.clear();
  instrument_stats_.clear();
  min_price_             = std::numeric_limits<int64_t>::max();
  max_price_             = std::numeric_limits<int64_t>::min();
  maybe_bad_book_events_ = 0;
  bad_ts_recv_events_    = 0;
  ts_recv_regressions_   = 0;
  avg_delta_ns_          = 0.0;
}

// ---------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& os, const Statistics& s) {
  os << "Statistics\n"
     << "  total_events          = " << s.totalEvents() << "\n"
     << "  first_ts              = " << formatIso(s.firstTs()) << "\n"
     << "  last_ts               = " << formatIso(s.lastTs())  << "\n"
     << "  maybe_bad_book_events = " << s.maybeBadBookEvents() << "\n"
     << "  bad_ts_recv_events    = " << s.badTsRecvEvents()    << "\n"
     << "  ts_recv_regressions   = " << s.tsRecvRegressions()  << "\n"
     << "  average delta         = " << s.tsAvgDeltaNs()  << "\n";

  if (s.hasPrice()) {
    os << "  price_range           = [" << formatPrice(s.minPrice())
       << ", " << formatPrice(s.maxPrice()) << "]\n";
  } else {
    os << "  price_range           = -\n";
  }

  os << "  action_counts:\n";
  for (char c : {'A', 'M', 'C', 'R', 'T', 'F', 'N'}) {
    const auto count = s.actionCount(static_cast<Action>(c));
    if (count) os << "    " << c << " = " << count << "\n";
  }

  os << "  publisher_counts (" << s.publisherCounts().size() << "):\n";
  {
    std::vector<std::pair<uint16_t, uint64_t>> pubs(
        s.publisherCounts().begin(), s.publisherCounts().end());
    std::sort(pubs.begin(), pubs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& [pid, c] : pubs)
      os << "    " << pid << " = " << c << "\n";
  }

  os << "  instruments (" << s.instrumentStats().size()
     << ") — top 10 by event count:\n";
  {
    std::vector<std::pair<uint32_t, const Statistics::InstrumentStats*>> v;
    v.reserve(s.instrumentStats().size());
    for (const auto& [id, is] : s.instrumentStats()) v.emplace_back(id, &is);
    std::sort(v.begin(), v.end(),
              [](const auto& a, const auto& b) {
                return a.second->count > b.second->count;
              });
    const std::size_t n = std::min<std::size_t>(10, v.size());
    for (std::size_t i = 0; i < n; ++i) {
      const auto& [id, is] = v[i];
      os << "    " << id
         << " [" << is->symbol << "]"
         << " count=" << is->count
         << " px=[" << formatPrice(is->min_price)
         << "," << formatPrice(is->max_price) << "]"
         << " regressions=" << is->ts_recv_regressions
         << "\n";
    }
  }

  return os;
}

} // namespace cmf
