#include "book/LimitOrderBook.hpp"

#include <algorithm>
#include <limits>
#include <ostream>
#include <string>

namespace md {
namespace {

bool hasValidSide(Side side) {
    return side == Side::Bid || side == Side::Ask;
}

bool hasValidPrice(std::int64_t price) {
    return price != std::numeric_limits<std::int64_t>::max();
}

bool hasValidRestingState(const MarketDataEvent& event) {
    return event.order_id != 0
        && hasValidSide(event.side)
        && hasValidPrice(event.price)
        && event.size > 0;
}

std::string formatOptionalPrice(std::optional<std::int64_t> price) {
    return price.has_value() ? formatPrice(*price) : "<none>";
}

} // namespace

LimitOrderBook::LimitOrderBook(std::uint64_t instrument_id)
    : instrument_id_(instrument_id) {}

void LimitOrderBook::apply(const MarketDataEvent& event) {
    switch (event.action) {
        case Action::Add:
            applyAdd(event);
            break;
        case Action::Modify:
            applyModify(event);
            break;
        case Action::Cancel:
            applyCancel(event);
            break;
        case Action::Clear:
            applyClear(event);
            break;
        case Action::Trade:
            applyTrade(event);
            break;
        case Action::Fill:
            applyFill(event);
            break;
        case Action::None:
            break;
    }
}

void LimitOrderBook::applyAdd(const MarketDataEvent& event) {
    if (!hasValidRestingState(event)) {
        return;
    }

    removeOrder(event.order_id);

    orders_[event.order_id] = RestingOrder{
        .side = event.side,
        .price = event.price,
        .size = event.size,
    };
    addLevelVolume(event.side, event.price, event.size);
}

void LimitOrderBook::applyCancel(const MarketDataEvent& event) {
    const auto it = orders_.find(event.order_id);
    if (it == orders_.end()) {
        ++skipped_unknown_order_count_;
        return;
    }

    auto& order = it->second;
    const auto cancel_size = event.size == 0
        ? order.size
        : std::min(event.size, order.size);

    removeLevelVolume(order.side, order.price, cancel_size);
    order.size -= cancel_size;

    if (order.size == 0) {
        orders_.erase(it);
    }
}

void LimitOrderBook::applyModify(const MarketDataEvent& event) {
    const auto it = orders_.find(event.order_id);
    if (it == orders_.end()) {
        if (hasValidRestingState(event)) {
            applyAdd(event);
        } else {
            ++skipped_unknown_order_count_;
        }
        return;
    }

    if (!hasValidRestingState(event)) {
        return;
    }

    removeLevelVolume(it->second.side, it->second.price, it->second.size);
    it->second = RestingOrder{
        .side = event.side,
        .price = event.price,
        .size = event.size,
    };
    addLevelVolume(event.side, event.price, event.size);
}

void LimitOrderBook::applyClear(const MarketDataEvent& event) {
    (void)event;

    bids_.clear();
    asks_.clear();
    orders_.clear();
}

void LimitOrderBook::applyTrade(const MarketDataEvent& event) {
    (void)event;
    ++trade_count_;
}

void LimitOrderBook::applyFill(const MarketDataEvent& event) {
    (void)event;
    ++fill_count_;
}

void LimitOrderBook::removeOrder(std::uint64_t order_id) {
    const auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        return;
    }

    removeLevelVolume(it->second.side, it->second.price, it->second.size);
    orders_.erase(it);
}

void LimitOrderBook::addLevelVolume(Side side, std::int64_t price, std::uint64_t size) {
    if (side == Side::Bid) {
        bids_[price] += size;
    } else if (side == Side::Ask) {
        asks_[price] += size;
    }
}

void LimitOrderBook::removeLevelVolume(Side side, std::int64_t price, std::uint64_t size) {
    if (side == Side::Bid) {
        const auto it = bids_.find(price);
        if (it == bids_.end()) {
            return;
        }
        if (it->second <= size) {
            bids_.erase(it);
        } else {
            it->second -= size;
        }
    } else if (side == Side::Ask) {
        const auto it = asks_.find(price);
        if (it == asks_.end()) {
            return;
        }
        if (it->second <= size) {
            asks_.erase(it);
        } else {
            it->second -= size;
        }
    }
}

std::optional<std::int64_t> LimitOrderBook::bestBid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->first;
}

std::optional<std::int64_t> LimitOrderBook::bestAsk() const {
    if (asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first;
}

std::uint64_t LimitOrderBook::volumeAt(Side side, std::int64_t price) const {
    if (side == Side::Bid) {
        const auto it = bids_.find(price);
        return it == bids_.end() ? 0 : it->second;
    }

    if (side == Side::Ask) {
        const auto it = asks_.find(price);
        return it == asks_.end() ? 0 : it->second;
    }

    return 0;
}

std::size_t LimitOrderBook::restingOrderCount() const noexcept {
    return orders_.size();
}

std::size_t LimitOrderBook::skippedUnknownOrderCount() const noexcept {
    return skipped_unknown_order_count_;
}

std::size_t LimitOrderBook::tradeCount() const noexcept {
    return trade_count_;
}

std::size_t LimitOrderBook::fillCount() const noexcept {
    return fill_count_;
}

std::uint64_t LimitOrderBook::instrumentId() const noexcept {
    return instrument_id_;
}

bool LimitOrderBook::containsOrder(std::uint64_t order_id) const noexcept {
    return orders_.find(order_id) != orders_.end();
}

std::vector<std::pair<std::int64_t, std::uint64_t>> LimitOrderBook::bidLevels() const {
    return {bids_.begin(), bids_.end()};
}

std::vector<std::pair<std::int64_t, std::uint64_t>> LimitOrderBook::askLevels() const {
    return {asks_.begin(), asks_.end()};
}

std::vector<std::pair<std::int64_t, std::uint64_t>> LimitOrderBook::bidLevels(std::size_t depth) const {
    std::vector<std::pair<std::int64_t, std::uint64_t>> levels;
    levels.reserve(std::min(depth, bids_.size()));
    std::size_t copied = 0;
    for (const auto& level : bids_) {
        if (copied++ >= depth) {
            break;
        }
        levels.push_back(level);
    }
    return levels;
}

std::vector<std::pair<std::int64_t, std::uint64_t>> LimitOrderBook::askLevels(std::size_t depth) const {
    std::vector<std::pair<std::int64_t, std::uint64_t>> levels;
    levels.reserve(std::min(depth, asks_.size()));
    std::size_t copied = 0;
    for (const auto& level : asks_) {
        if (copied++ >= depth) {
            break;
        }
        levels.push_back(level);
    }
    return levels;
}

void LimitOrderBook::printSnapshot(std::ostream& out, std::size_t depth) const {
    out << "instrument_id=" << instrument_id_
        << " resting_orders=" << orders_.size()
        << " best_bid=" << formatOptionalPrice(bestBid())
        << " best_ask=" << formatOptionalPrice(bestAsk()) << '\n';

    out << "  bids:\n";
    std::size_t printed = 0;
    for (const auto& [price, volume] : bids_) {
        if (printed++ >= depth) {
            break;
        }
        out << "    " << formatPrice(price) << " x " << volume << '\n';
    }

    out << "  asks:\n";
    printed = 0;
    for (const auto& [price, volume] : asks_) {
        if (printed++ >= depth) {
            break;
        }
        out << "    " << formatPrice(price) << " x " << volume << '\n';
    }
}

} // namespace md
