#include "lob/InstrumentBookRegistry.hpp"

namespace cmf {

const LimitOrderBook *
InstrumentBookRegistry::find(InstrumentId iid) const noexcept {
  auto it = books_.find(iid);
  return it == books_.end() ? nullptr : &it->second;
}

LimitOrderBook *InstrumentBookRegistry::find(InstrumentId iid) noexcept {
  auto it = books_.find(iid);
  return it == books_.end() ? nullptr : &it->second;
}

InstrumentBookRegistry::RouteResult
InstrumentBookRegistry::apply(const MarketDataEvent &ev) {
  // Step 1: figure out which instrument this event belongs to.
  //
  // The feed always supplies instrument_id on Add (and in practice on every
  // event), but we still fall back to the order_id -> iid cache because:
  //   - Defensive: some malformed records lack the field.
  //   - Robust against vendor flows where Cancel/Modify omit context.
  InstrumentId iid = ev.instrument_id;

  if (ev.action == Action::Cancel ||
      ev.action == Action::Modify ||
      ev.action == Action::Fill) {
    if (iid == 0) {
      auto it = order_to_iid_.find(ev.order_id);
      if (it == order_to_iid_.end()) {
        ++n_unknown_order_;
        return RouteResult::UnknownOrder;
      }
      iid = it->second;
    }
  }

  if (iid == 0 && ev.action != Action::Clear) {
    // Clear without any context falls through to "broadcast"; everything else
    // we can't route is dropped.
    ++n_unroutable_;
    return RouteResult::Unroutable;
  }

  // Step 2: route into the right book (creating it on first use).
  if (ev.action == Action::Clear && iid == 0) {
    // Session-wide reset: clear every known book. Treated as "applied".
    for (auto &[_, book] : books_)
      book.apply(ev);
    order_to_iid_.clear();
    ++n_routed_;
    return RouteResult::Applied;
  }

  auto &book = books_[iid];
  book.apply(ev);

  // Step 3: maintain the order_id -> iid cache. We mirror the bookkeeping
  // that LimitOrderBook already performs internally to keep both views in
  // sync without a second pass.
  switch (ev.action) {
  case Action::Add:
    order_to_iid_[ev.order_id] = iid;
    break;
  case Action::Cancel:
  case Action::Fill:
    if (book.orders().find(ev.order_id) == book.orders().end())
      order_to_iid_.erase(ev.order_id);
    break;
  case Action::Modify:
    if (book.orders().find(ev.order_id) == book.orders().end())
      order_to_iid_.erase(ev.order_id); // Modify-to-zero == cancel
    break;
  case Action::Clear:
    // Per-instrument Clear: drop only that instrument's order ids.
    for (auto it = order_to_iid_.begin(); it != order_to_iid_.end();) {
      if (it->second == iid)
        it = order_to_iid_.erase(it);
      else
        ++it;
    }
    break;
  default:
    break;
  }

  ++n_routed_;
  return RouteResult::Applied;
}

} // namespace cmf
