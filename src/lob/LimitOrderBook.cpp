#include "lob/LimitOrderBook.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <ostream>

namespace cmf {

// ---------------------------------------------------------------------------
// Tick conversion

LimitOrderBook::Tick LimitOrderBook::toTick(double price) noexcept {
  // std::llround handles both signs and rounds half away from zero, which
  // is exactly what we want for prices that come back as a textual
  // representation of an exact 1e-9 multiple.
  return static_cast<Tick>(std::llround(price * kPriceScale));
}

double LimitOrderBook::fromTick(Tick tick) noexcept {
  return static_cast<double>(tick) / kPriceScale;
}

// ---------------------------------------------------------------------------
// Level helpers

void LimitOrderBook::addLevel(MdSide side, Tick tick, Qty qty) {
  if (qty <= 0)
    return;
  if (side == MdSide::Bid) {
    bids_[tick] += qty;
  } else if (side == MdSide::Ask) {
    asks_[tick] += qty;
  }
}

void LimitOrderBook::removeLevel(MdSide side, Tick tick, Qty qty) {
  if (qty <= 0)
    return;
  auto eraseIfEmpty = [](auto &map, auto it, Qty q) {
    it->second -= q;
    // Defensive: clamp to zero in case of upstream inconsistency. Strictly
    // negative residuals should not happen on a well-formed feed but we'd
    // rather drop the level than corrupt the entire side.
    if (it->second <= 0)
      map.erase(it);
  };
  if (side == MdSide::Bid) {
    auto it = bids_.find(tick);
    if (it != bids_.end())
      eraseIfEmpty(bids_, it, qty);
  } else if (side == MdSide::Ask) {
    auto it = asks_.find(tick);
    if (it != asks_.end())
      eraseIfEmpty(asks_, it, qty);
  }
}

// ---------------------------------------------------------------------------
// apply()

void LimitOrderBook::apply(const MarketDataEvent &ev) {
  switch (ev.action) {
  // --- Add a new resting order -------------------------------------------
  case Action::Add: {
    if (ev.side == MdSide::None || !priceDefined(ev.price) || ev.size <= 0) {
      ++n_skipped_;
      return;
    }
    const Tick tick = toTick(ev.price);
    const Qty  qty  = static_cast<Qty>(std::llround(ev.size));
    // If the same order_id is reused (vendor edge case) we treat it as a
    // pure overwrite: subtract the old contribution from the level first.
    if (auto it = orders_.find(ev.order_id); it != orders_.end()) {
      removeLevel(it->second.side, it->second.tick, it->second.qty);
      it->second = RestingOrder{tick, qty, ev.side};
    } else {
      orders_.emplace(ev.order_id, RestingOrder{tick, qty, ev.side});
    }
    addLevel(ev.side, tick, qty);
    ++n_add_;
    return;
  }

  // --- Modify (price and/or quantity change) -----------------------------
  case Action::Modify: {
    auto it = orders_.find(ev.order_id);
    if (it == orders_.end() || !priceDefined(ev.price)) {
      ++n_skipped_;
      return;
    }
    const Tick new_tick = toTick(ev.price);
    const Qty  new_qty  = static_cast<Qty>(std::llround(ev.size));
    if (new_qty <= 0) {
      // Treat Modify with zero size as a full cancel.
      removeLevel(it->second.side, it->second.tick, it->second.qty);
      orders_.erase(it);
    } else if (new_tick == it->second.tick) {
      // Same price level: adjust by the delta only.
      const Qty delta = new_qty - it->second.qty;
      if (delta > 0)
        addLevel(it->second.side, new_tick, delta);
      else if (delta < 0)
        removeLevel(it->second.side, new_tick, -delta);
      it->second.qty = new_qty;
    } else {
      // Price moved: pull the entire qty off the old level, post on the new.
      removeLevel(it->second.side, it->second.tick, it->second.qty);
      addLevel(it->second.side, new_tick, new_qty);
      it->second.tick = new_tick;
      it->second.qty  = new_qty;
    }
    ++n_modify_;
    return;
  }

  // --- Cancel (full or partial) ------------------------------------------
  case Action::Cancel: {
    auto it = orders_.find(ev.order_id);
    if (it == orders_.end()) {
      ++n_skipped_;
      return;
    }
    const Qty cancelled =
        ev.size > 0
            ? std::min(it->second.qty, static_cast<Qty>(std::llround(ev.size)))
            : it->second.qty; // size==0 means cancel-remaining
    removeLevel(it->second.side, it->second.tick, cancelled);
    it->second.qty -= cancelled;
    if (it->second.qty <= 0)
      orders_.erase(it);
    ++n_cancel_;
    return;
  }

  // --- Fill: matched against a resting order, behaves like a partial cancel
  case Action::Fill: {
    auto it = orders_.find(ev.order_id);
    if (it == orders_.end()) {
      // Aggressor-side fill (no resting order) is normal in MBO; counted but
      // it doesn't change the book.
      ++n_fill_;
      return;
    }
    const Qty filled =
        ev.size > 0
            ? std::min(it->second.qty, static_cast<Qty>(std::llround(ev.size)))
            : it->second.qty;
    removeLevel(it->second.side, it->second.tick, filled);
    it->second.qty -= filled;
    if (it->second.qty <= 0)
      orders_.erase(it);
    ++n_fill_;
    return;
  }

  // --- Trade: pure tape-print, no book mutation ---------------------------
  case Action::Trade:
    ++n_trade_;
    return;

  // --- Clear: snapshot reset ---------------------------------------------
  case Action::Clear:
    clearBook();
    ++n_clear_;
    return;

  // --- Unknown -----------------------------------------------------------
  case Action::None:
  default:
    ++n_skipped_;
    return;
  }
}

void LimitOrderBook::clearBook() noexcept {
  bids_.clear();
  asks_.clear();
  orders_.clear();
}

// ---------------------------------------------------------------------------
// BBO accessors

double LimitOrderBook::bestBidPrice() const noexcept {
  return fromTick(bids_.begin()->first);
}
double LimitOrderBook::bestAskPrice() const noexcept {
  return fromTick(asks_.begin()->first);
}
LimitOrderBook::Qty LimitOrderBook::bestBidQty() const noexcept {
  return bids_.begin()->second;
}
LimitOrderBook::Qty LimitOrderBook::bestAskQty() const noexcept {
  return asks_.begin()->second;
}

// ---------------------------------------------------------------------------
// Top-N

std::vector<std::pair<double, LimitOrderBook::Qty>>
LimitOrderBook::topBids(std::size_t n) const {
  std::vector<std::pair<double, Qty>> out;
  out.reserve(std::min(n, bids_.size()));
  for (const auto &[tick, qty] : bids_) {
    if (out.size() >= n)
      break;
    out.emplace_back(fromTick(tick), qty);
  }
  return out;
}

LimitOrderBook::Qty
LimitOrderBook::volumeAtPrice(MdSide side, double price) const noexcept {
  const Tick t = toTick(price);
  if (side == MdSide::Bid) {
    auto it = bids_.find(t);
    return it == bids_.end() ? Qty{0} : it->second;
  }
  if (side == MdSide::Ask) {
    auto it = asks_.find(t);
    return it == asks_.end() ? Qty{0} : it->second;
  }
  return 0;
}

std::vector<std::pair<double, LimitOrderBook::Qty>>
LimitOrderBook::topAsks(std::size_t n) const {
  std::vector<std::pair<double, Qty>> out;
  out.reserve(std::min(n, asks_.size()));
  for (const auto &[tick, qty] : asks_) {
    if (out.size() >= n)
      break;
    out.emplace_back(fromTick(tick), qty);
  }
  return out;
}

// ---------------------------------------------------------------------------
// Pretty print

void LimitOrderBook::printSnapshot(std::ostream &os, std::size_t depth) const {
  os << "  bids (" << bids_.size() << " lvls):";
  std::size_t k = 0;
  for (const auto &[tick, qty] : bids_) {
    if (k++ >= depth) break;
    os << ' ' << std::fixed << std::setprecision(5) << fromTick(tick)
       << '@' << qty;
  }
  os << '\n';
  os << "  asks (" << asks_.size() << " lvls):";
  k = 0;
  for (const auto &[tick, qty] : asks_) {
    if (k++ >= depth) break;
    os << ' ' << std::fixed << std::setprecision(5) << fromTick(tick)
       << '@' << qty;
  }
  os << '\n';
}

} // namespace cmf
