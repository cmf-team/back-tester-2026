// Sequential dispatcher that routes merged events into one LOB per instrument.

#pragma once

#include "lob/LimitOrderBook.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cmf {

struct DispatchOptions {
  std::size_t snapshot_every = 100'000;
  std::size_t max_snapshots = 3;
  std::size_t snapshot_depth = 5;
};

struct DispatchStats {
  std::size_t total_events = 0;
  std::size_t instruments = 0;
  std::size_t snapshots = 0;
  std::size_t missing_order_events = 0;
  std::size_t ignored_events = 0;
  std::size_t unresolved_routes = 0;
  std::size_t ambiguous_routes = 0;
  std::size_t adds = 0;
  std::size_t cancels = 0;
  std::size_t modifies = 0;
  std::size_t clears = 0;
  std::size_t trades = 0;
  std::size_t fills = 0;
  std::size_t none = 0;
};

struct BookSummary {
  std::uint32_t instrument_id{0};
  std::size_t orders = 0;
  std::size_t bid_levels = 0;
  std::size_t ask_levels = 0;
  std::optional<PriceLevel> best_bid;
  std::optional<PriceLevel> best_ask;
  std::string snapshot;
};

struct CapturedSnapshot {
  std::size_t event_index = 0;
  std::uint64_t ts_recv = UNDEF_TIMESTAMP;
  BookSummary book;
};

class BookDispatcher {
public:
  explicit BookDispatcher(DispatchOptions options = {});

  void apply(const MarketDataEvent &event);

  [[nodiscard]] const DispatchStats &stats() const noexcept { return stats_; }
  [[nodiscard]] const std::vector<CapturedSnapshot> &snapshots() const noexcept {
    return snapshots_;
  }
  [[nodiscard]] std::vector<BookSummary>
  finalSummaries(std::size_t depth = 1) const;

private:
  LimitOrderBook &getOrCreateBook(std::uint32_t instrument_id);
  std::optional<std::uint32_t>
  resolveInstrumentId(const MarketDataEvent &event, bool &ambiguous) const;
  [[nodiscard]] BookSummary summarise(const LimitOrderBook &book,
                                      std::size_t depth) const;
  void maybeCaptureSnapshot(const LimitOrderBook &book,
                            const MarketDataEvent &event);

  DispatchOptions options_{};
  DispatchStats stats_{};
  std::map<std::uint32_t, LimitOrderBook> books_;
  std::vector<CapturedSnapshot> snapshots_;
};

} // namespace cmf
