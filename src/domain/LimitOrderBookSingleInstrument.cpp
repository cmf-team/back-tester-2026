#include "domain/LimitOrderBookSingleInstrument.hpp"

#include <algorithm>
#include <iostream>

namespace domain {
void LimitOrderBookSingleInstrument::onAdd(const MarketDataEvent &event) {
  if (!isBookSide(event.side) || event.size <= 0) {
    return;
  }

  if (const auto it = orders_.find(event.order_id); it != orders_.end()) {
    removeOrderContribution(it->second);
    orders_.erase(it);
  }

  const OrderState state{event.side, static_cast<Price>(event.price),
                         static_cast<Quantity>(event.size)};
  orders_[event.order_id] = state;
  addOrderContribution(state);
}

void LimitOrderBookSingleInstrument::onModify(const MarketDataEvent &event) {
  const auto it = orders_.find(event.order_id);
  if (it == orders_.end()) {
    onAdd(event);
    return;
  }

  removeOrderContribution(it->second);

  const char new_side = isBookSide(event.side) ? event.side : it->second.side;
  const Quantity new_size = static_cast<Quantity>(event.size);
  if (new_size <= 0 || !isBookSide(new_side)) {
    orders_.erase(it);
    return;
  }

  it->second.side = new_side;
  it->second.price = event.price;
  it->second.size = new_size;
  addOrderContribution(it->second);
}

void LimitOrderBookSingleInstrument::onCancel(const MarketDataEvent &event) {
  const auto it = orders_.find(event.order_id);
  if (it == orders_.end()) {
    return;
  }

  const Quantity cancel_size = static_cast<Quantity>(event.size);
  if (cancel_size <= 0) {
    return;
  }

  const Quantity removed = std::min(it->second.size, cancel_size);
  if (removed <= 0) {
    return;
  }

  adjustLevel(it->second.side, it->second.price, -removed);
  it->second.size -= removed;
  if (it->second.size <= 0) {
    orders_.erase(it);
  }
}

void LimitOrderBookSingleInstrument::onTrade(const MarketDataEvent &event) {
  updateExecStats(trade_stats_, event);
}

void LimitOrderBookSingleInstrument::onFill(const MarketDataEvent &event) {
  updateExecStats(fill_stats_, event);
}

void LimitOrderBookSingleInstrument::onClear() {
  bids_.clear();
  asks_.clear();
  orders_.clear();
  trade_stats_ = {};
  fill_stats_ = {};
}

void LimitOrderBookSingleInstrument::updateExecStats(
    ExecStats &stats, const MarketDataEvent &event) {
  stats.count += 1;
  const auto size = static_cast<Quantity>(event.size);
  stats.total_size += size;
  stats.last_size = size;
  stats.last_side = event.side;
  if (event.price != domain::events::UNDEF_PRICE) {
    stats.last_price = static_cast<Price>(event.price);
  }
}

void LimitOrderBookSingleInstrument::onEvent(const MarketDataEvent &event) {
  std::unique_lock<std::mutex> lock(m_);

  switch (event.action) {
  case 'A':
    onAdd(event);
    break;
  case 'M':
    onModify(event);
    break;
  case 'C':
    onCancel(event);
    break;
  case 'T':
    onTrade(event);
    break;
  case 'F':
    onFill(event);
    break;
  case 'R':
    onClear();
    break;
  default:
    break;
  }
}

BidsBookMap LimitOrderBookSingleInstrument::getBids() const {
  std::lock_guard<std::mutex> lock(m_);
  return bids_;
}

AsksBookMap LimitOrderBookSingleInstrument::getAsks() const {
  std::lock_guard<std::mutex> lock(m_);
  return asks_;
}

BestQuote LimitOrderBookSingleInstrument::getBestBid() const {
  std::lock_guard<std::mutex> lock(m_);
  if (bids_.empty()) {
    return std::nullopt;
  }
  return *bids_.begin();
}

BestQuote LimitOrderBookSingleInstrument::getBestAsk() const {
  std::lock_guard<std::mutex> lock(m_);
  if (asks_.empty()) {
    return std::nullopt;
  }
  return *asks_.begin();
}

Quantity LimitOrderBookSingleInstrument::getVolumeAtPrice(const char side,
                                                         const Price price) const {
  std::lock_guard<std::mutex> lock(m_);
  if (side == 'B') {
    const auto it = bids_.find(price);
    return it == bids_.end() ? Quantity{0} : it->second;
  }
  if (side == 'A') {
    const auto it = asks_.find(price);
    return it == asks_.end() ? Quantity{0} : it->second;
  }
  return Quantity{0};
}

ExecStats LimitOrderBookSingleInstrument::getTradeStats() const {
  const std::lock_guard<std::mutex> lock(m_);
  return trade_stats_;
}

ExecStats LimitOrderBookSingleInstrument::getFillStats() const {
  const std::lock_guard<std::mutex> lock(m_);
  return fill_stats_;
}

bool LimitOrderBookSingleInstrument::isBookSide(const char side) {
  return side == 'B' || side == 'A';
}

void LimitOrderBookSingleInstrument::addOrderContribution(
    const OrderState &state) {
  adjustLevel(state.side, state.price, state.size);
}

void LimitOrderBookSingleInstrument::removeOrderContribution(
    const OrderState &state) {
  adjustLevel(state.side, state.price, -state.size);
}

void LimitOrderBookSingleInstrument::adjustLevel(const char side,
                                                 const Price price,
                                                 const Quantity delta) {
  if (delta == 0) {
    return;
  }

  auto adjust = [&](auto &book) {
    auto &level = book[price];
    level += delta;
    if (level <= 0) {
      book.erase(price);
    }
  };

  if (side == 'B') {
    adjust(bids_);
  } else if (side == 'A') {
    adjust(asks_);
  }
}
} 
