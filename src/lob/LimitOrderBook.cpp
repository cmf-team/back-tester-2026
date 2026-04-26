#include "lob/LimitOrderBook.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace cmf {

namespace {

constexpr std::uint64_t kPriceScale = 1'000'000'000ULL;

template <class Levels>
void adjustLevel(Levels &levels, std::int64_t price, std::int64_t delta) {
  if (price == UNDEF_PRICE || delta == 0) {
    return;
  }
  auto it = levels.find(price);
  if (delta > 0) {
    if (it == levels.end()) {
      levels.emplace(price, static_cast<std::uint64_t>(delta));
    } else {
      it->second += static_cast<std::uint64_t>(delta);
    }
    return;
  }
  if (it == levels.end()) {
    return;
  }
  const auto removal = static_cast<std::uint64_t>(-delta);
  if (removal >= it->second) {
    levels.erase(it);
    return;
  }
  it->second -= removal;
}

std::string formatPrice(std::int64_t price) {
  if (price == UNDEF_PRICE) {
    return "undef";
  }
  const bool negative = price < 0;
  const auto magnitude = negative
                             ? static_cast<std::uint64_t>(-(price + 1)) + 1ULL
                             : static_cast<std::uint64_t>(price);
  const auto whole = magnitude / kPriceScale;
  const auto fractional = magnitude % kPriceScale;

  std::ostringstream out;
  if (negative) {
    out << '-';
  }
  out << whole << '.' << std::setw(9) << std::setfill('0') << fractional;
  return out.str();
}

template <class Levels>
std::string formatLevels(const Levels &levels, std::size_t depth) {
  std::ostringstream out;
  out << '[';
  std::size_t index = 0;
  for (const auto &[price, size] : levels) {
    if (index == depth) {
      break;
    }
    if (index != 0) {
      out << ", ";
    }
    out << formatPrice(price) << 'x' << size;
    ++index;
  }
  out << ']';
  return out.str();
}

std::string formatTop(const std::optional<PriceLevel> &level) {
  if (!level.has_value()) {
    return "none";
  }
  return formatPrice(level->price) + 'x' + std::to_string(level->size);
}

} // namespace

LimitOrderBook::LimitOrderBook(std::uint32_t instrument_id)
    : instrument_id_(instrument_id) {}

ApplyResult LimitOrderBook::apply(const MarketDataEvent &event) {
  switch (event.action) {
  case MdAction::Add:
    return applyAdd(event);
  case MdAction::Cancel:
    return applyCancel(event);
  case MdAction::Modify:
    return applyModify(event);
  case MdAction::Clear:
    bids_.clear();
    asks_.clear();
    orders_.clear();
    return {};
  case MdAction::Trade:
  case MdAction::Fill:
  case MdAction::None:
    return {};
  }
  return {};
}

std::optional<PriceLevel> LimitOrderBook::bestBid() const noexcept {
  if (bids_.empty()) {
    return std::nullopt;
  }
  const auto &[price, size] = *bids_.begin();
  return PriceLevel{price, size};
}

std::optional<PriceLevel> LimitOrderBook::bestAsk() const noexcept {
  if (asks_.empty()) {
    return std::nullopt;
  }
  const auto &[price, size] = *asks_.begin();
  return PriceLevel{price, size};
}

std::uint64_t LimitOrderBook::volumeAtPrice(MdSide side,
                                            std::int64_t price) const noexcept {
  if (side == MdSide::Bid) {
    const auto it = bids_.find(price);
    return it == bids_.end() ? 0 : it->second;
  }
  if (side == MdSide::Ask) {
    const auto it = asks_.find(price);
    return it == asks_.end() ? 0 : it->second;
  }
  return 0;
}

bool LimitOrderBook::hasOrder(std::uint16_t publisher_id,
                              std::uint64_t order_id) const noexcept {
  return orders_.contains(OrderKey{publisher_id, order_id});
}

std::string LimitOrderBook::snapshotString(std::size_t depth) const {
  std::ostringstream out;
  out << "instrument=" << instrument_id_ << " orders=" << orderCount()
      << " bid_levels=" << bidLevelCount() << " ask_levels=" << askLevelCount()
      << " best_bid=" << formatTop(bestBid())
      << " best_ask=" << formatTop(bestAsk()) << " bids="
      << formatLevels(bids_, depth) << " asks=" << formatLevels(asks_, depth);
  return out.str();
}

std::size_t
LimitOrderBook::OrderKeyHash::operator()(const OrderKey &key) const noexcept {
  const std::size_t lhs = std::hash<std::uint16_t>{}(key.publisher_id);
  const std::size_t rhs = std::hash<std::uint64_t>{}(key.order_id);
  return lhs ^ (rhs + 0x9e3779b97f4a7c15ULL + (lhs << 6U) + (lhs >> 2U));
}

ApplyResult LimitOrderBook::applyAdd(const MarketDataEvent &event) {
  if (!isRestingOrder(event.side, event.price, event.size)) {
    return {.ignored = true};
  }

  const OrderKey key = makeOrderKey(event);
  const auto existing = orders_.find(key);
  if (existing != orders_.end()) {
    eraseOrder(existing->first, existing->second);
  }
  addOrder(key, OrderState{event.price, event.size, event.side});
  return {};
}

ApplyResult LimitOrderBook::applyCancel(const MarketDataEvent &event) {
  const OrderKey key = makeOrderKey(event);
  const auto it = orders_.find(key);
  if (it == orders_.end()) {
    return {.missing_order = true};
  }

  OrderState updated = it->second;
  const std::uint32_t cancel_size =
      event.size == 0 ? updated.size : std::min(event.size, updated.size);
  eraseOrder(it->first, it->second);

  if (cancel_size < updated.size) {
    updated.size -= cancel_size;
    addOrder(key, updated);
  }
  return {};
}

ApplyResult LimitOrderBook::applyModify(const MarketDataEvent &event) {
  const OrderKey key = makeOrderKey(event);
  const auto it = orders_.find(key);
  if (it == orders_.end()) {
    return {.missing_order = true};
  }

  OrderState updated = it->second;
  if (event.side != MdSide::None) {
    updated.side = event.side;
  }
  if (event.price != UNDEF_PRICE) {
    updated.price = event.price;
  }
  updated.size = event.size;

  eraseOrder(it->first, it->second);
  if (!isRestingOrder(updated.side, updated.price, updated.size)) {
    return {};
  }
  addOrder(key, updated);
  return {};
}

void LimitOrderBook::addOrder(const OrderKey &key, const OrderState &state) {
  orders_[key] = state;
  if (state.side == MdSide::Bid) {
    adjustLevel(bids_, state.price, static_cast<std::int64_t>(state.size));
  } else if (state.side == MdSide::Ask) {
    adjustLevel(asks_, state.price, static_cast<std::int64_t>(state.size));
  }
}

void LimitOrderBook::eraseOrder(const OrderKey &key, const OrderState &state) {
  if (state.side == MdSide::Bid) {
    adjustLevel(bids_, state.price, -static_cast<std::int64_t>(state.size));
  } else if (state.side == MdSide::Ask) {
    adjustLevel(asks_, state.price, -static_cast<std::int64_t>(state.size));
  }
  orders_.erase(key);
}

bool LimitOrderBook::isRestingOrder(MdSide side, std::int64_t price,
                                    std::uint32_t size) noexcept {
  return (side == MdSide::Bid || side == MdSide::Ask) && price != UNDEF_PRICE &&
         size > 0;
}

LimitOrderBook::OrderKey
LimitOrderBook::makeOrderKey(const MarketDataEvent &event) noexcept {
  return OrderKey{event.publisher_id, event.order_id};
}

} // namespace cmf
