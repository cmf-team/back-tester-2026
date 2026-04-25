// LimitOrderBook: per-instrument L2-aggregated limit order book.
//
// Design notes (HW-2):
//
//   * Prices are stored as int64_t "ticks" scaled by 1e9 (Databento's native
//     fixed-point unit). This buys us deterministic ordering and exact
//     comparisons inside std::map without paying for double round-off.
//
//   * Quantities are kept as int64_t. Vendor sizes are integral; using int64
//     here removes the risk of slowly drifting aggregates when many
//     fractional doubles get added/subtracted.
//
//   * The L3 (MBO) feed delivers every order separately, so we maintain
//     a side table `orders_` mapping order_id -> {side, tick, qty}. This is
//     required because:
//       - Cancel / Modify / Fill events frequently arrive with side=None,
//         so we must look it up from the original Add.
//       - Modify must subtract the order's previous contribution from its
//         old price level and add it to a (possibly different) new level.
//
//   * Two maps for the book itself:
//       std::map<Tick, Qty, std::greater<Tick>> bids_ (best at .begin())
//       std::map<Tick, Qty, std::less<Tick>>    asks_ (best at .begin())
//     Uniform begin()-is-best access simplifies BBO and top-N queries.
//
//   * Trade events do not modify the book (matched volume is reflected by
//     the corresponding Fill events that follow), but we track the count
//     and notional volume for diagnostics.
//
// Non-goals: thread safety. The book is single-writer by construction;
// the surrounding pipeline (Dispatcher / ShardedDispatcher) guarantees
// that exactly one thread mutates a given book at a time. Snapshotting
// either runs on the same thread (safe by definition) or quiesces the
// owning worker via InstrumentBookRegistry::freeze() (see Snapshotter).

#pragma once

#include "parser/MarketDataEvent.hpp"

#include <cstdint>
#include <iosfwd>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cmf {

class LimitOrderBook {
public:
  using Tick = std::int64_t;
  using Qty  = std::int64_t;

  // Databento prices are reported with 1e-9 precision. We round-half-away-
  // from-zero to be robust against small textual round-trips.
  static constexpr double kPriceScale = 1e9;
  static Tick   toTick(double price) noexcept;
  static double fromTick(Tick tick)  noexcept;

  // Apply a single market data event. Unknown / inconsistent events are
  // counted in `skipped_` and silently dropped (see counters below).
  void apply(const MarketDataEvent &ev);

  // Reset the entire book and per-order map. Counters are NOT reset, so
  // they keep aggregating across the whole session (Clear is itself an
  // event that the operator may want to count).
  void clearBook() noexcept;

  // BBO ---------------------------------------------------------------------
  bool hasBid() const noexcept { return !bids_.empty(); }
  bool hasAsk() const noexcept { return !asks_.empty(); }

  // Pre: hasBid()/hasAsk() respectively.
  double bestBidPrice() const noexcept;
  double bestAskPrice() const noexcept;
  Qty    bestBidQty()   const noexcept;
  Qty    bestAskQty()   const noexcept;

  // Top-N snapshot helpers (used by Snapshotter). Returned in best-first
  // order. Empty book yields empty vector.
  std::vector<std::pair<double, Qty>> topBids(std::size_t n) const;
  std::vector<std::pair<double, Qty>> topAsks(std::size_t n) const;

  // Aggregated quantity at the given price level on the requested side.
  // Returns 0 if the level is empty or `side` is None.
  Qty volumeAtPrice(MdSide side, double price) const noexcept;

  // Sizes -------------------------------------------------------------------
  std::size_t bidLevels()  const noexcept { return bids_.size(); }
  std::size_t askLevels()  const noexcept { return asks_.size(); }
  std::size_t openOrders() const noexcept { return orders_.size(); }

  // Per-action counters. Useful for both diagnostics and unit tests.
  std::uint64_t addCount()     const noexcept { return n_add_; }
  std::uint64_t cancelCount()  const noexcept { return n_cancel_; }
  std::uint64_t modifyCount()  const noexcept { return n_modify_; }
  std::uint64_t tradeCount()   const noexcept { return n_trade_; }
  std::uint64_t fillCount()    const noexcept { return n_fill_; }
  std::uint64_t clearCount()   const noexcept { return n_clear_; }
  std::uint64_t skippedCount() const noexcept { return n_skipped_; }

  // Pretty-prints first `depth` levels of each side (used by snapshots).
  void printSnapshot(std::ostream &os, std::size_t depth = 5) const;

  // Per-order record (kept public so tests can reach into it).
  struct RestingOrder {
    Tick   tick{};
    Qty    qty{};
    MdSide side{MdSide::None};
  };
  using OrderMap = std::unordered_map<OrderId, RestingOrder>;

  const OrderMap &orders() const noexcept { return orders_; }

private:
  // --- helpers ------------------------------------------------------------
  void addLevel(MdSide side, Tick tick, Qty qty);
  void removeLevel(MdSide side, Tick tick, Qty qty);

  // --- state --------------------------------------------------------------
  std::map<Tick, Qty, std::greater<Tick>> bids_;
  std::map<Tick, Qty, std::less<Tick>>    asks_;
  std::unordered_map<OrderId, RestingOrder> orders_;

  std::uint64_t n_add_{0}, n_cancel_{0}, n_modify_{0};
  std::uint64_t n_trade_{0}, n_fill_{0}, n_clear_{0}, n_skipped_{0};
};

} // namespace cmf
