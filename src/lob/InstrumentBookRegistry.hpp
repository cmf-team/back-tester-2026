// InstrumentBookRegistry: owns a set of LimitOrderBook instances keyed by
// instrument_id and routes each MarketDataEvent to the right one.
//
// Why a dedicated class:
//
//   * The vendor feed mixes events for many instruments in the same file.
//     Routing logic is non-trivial (Cancel/Modify/Fill events are sometimes
//     emitted without an instrument_id field, so we must remember which
//     instrument each open order belongs to).
//
//   * Keeping routing separate from LimitOrderBook lets us test the two
//     concerns in isolation, and lets ShardedDispatcher own multiple
//     registries (one per worker thread) with the exact same API.
//
//   * Snapshotter operates on a registry, not on a single book, so this
//     is the natural surface to expose iteration / freeze hooks.

#pragma once

#include "lob/LimitOrderBook.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace cmf {

using InstrumentId = std::uint32_t;

class InstrumentBookRegistry {
public:
  // Outcome of a single apply() call.
  enum class RouteResult {
    Applied,         // event was forwarded to the matching book
    UnknownOrder,    // Cancel/Modify/Fill arrived before any Add
    Unroutable,      // could not infer instrument_id at all
  };

  // Routes one event to the matching LimitOrderBook. The book is created on
  // first use. Returns the routing outcome (also reflected in counters).
  RouteResult apply(const MarketDataEvent &ev);

  // Number of distinct instruments seen so far.
  std::size_t size() const noexcept { return books_.size(); }

  // Find a book by instrument id. Returns nullptr if not present.
  const LimitOrderBook *find(InstrumentId iid) const noexcept;
  LimitOrderBook       *find(InstrumentId iid) noexcept;

  // Iterate all books (used by Snapshotter and reporting).
  template <class F> void forEach(F &&f) const {
    for (const auto &[iid, book] : books_)
      f(iid, book);
  }
  template <class F> void forEach(F &&f) {
    for (auto &[iid, book] : books_)
      f(iid, book);
  }

  // Counters --------------------------------------------------------------
  std::uint64_t routedCount()      const noexcept { return n_routed_; }
  std::uint64_t unknownOrderCount() const noexcept { return n_unknown_order_; }
  std::uint64_t unroutableCount()  const noexcept { return n_unroutable_; }

  std::size_t orderCacheSize() const noexcept { return order_to_iid_.size(); }

private:
  // Books keyed by instrument id. unordered_map gives O(1) routing; we
  // never iterate inside the hot loop (only in snapshot/report code).
  std::unordered_map<InstrumentId, LimitOrderBook> books_;

  // order_id -> instrument_id cache. Populated on Add, consulted on
  // Cancel/Modify/Fill, evicted whenever the order is fully removed.
  std::unordered_map<OrderId, InstrumentId> order_to_iid_;

  std::uint64_t n_routed_{0};
  std::uint64_t n_unknown_order_{0};
  std::uint64_t n_unroutable_{0};
};

} // namespace cmf
