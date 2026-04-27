#include "LimitOrderBook.hpp"

#include <cmath>
#include <limits>
#include <ostream>

namespace cmf {

namespace {

// helper: pick the right side map (mutating)
std::map<Price, Quantity>& levelMap(
    std::map<Price, Quantity>& bids,
    std::map<Price, Quantity>& asks,
    Side side
) {
    return (side == Side::Buy) ? bids : asks;
}

const std::map<Price, Quantity>& levelMap(
    const std::map<Price, Quantity>& bids,
    const std::map<Price, Quantity>& asks,
    Side side
) {
    return (side == Side::Buy) ? bids : asks;
}

bool validLevel(Price price, Quantity size) {
    // skip "no quote" markers and zero-size noise
    return !std::isnan(price) && size > 0.0;
}

constexpr Price NO_PRICE = std::numeric_limits<Price>::quiet_NaN();

} // anonymous namespace

void LimitOrderBook::addLiquidity(Side side, Price price, Quantity size) {
    if (!validLevel(price, size)) return;
    levelMap(bids_, asks_, side)[price] += size;
}

void LimitOrderBook::removeLiquidity(Side side, Price price, Quantity size) {
    if (!validLevel(price, size)) return;
    auto& m = levelMap(bids_, asks_, side);
    auto it = m.find(price);
    if (it == m.end()) return;
    it->second -= size;
    if (it->second <= 0.0) {
        m.erase(it);
    }
}

void LimitOrderBook::clearBook() {
    bids_.clear();
    asks_.clear();
}

Price LimitOrderBook::bestBid() const {
    return bids_.empty() ? NO_PRICE : bids_.rbegin()->first;
}

Price LimitOrderBook::bestAsk() const {
    return asks_.empty() ? NO_PRICE : asks_.begin()->first;
}

Quantity LimitOrderBook::bestBidSize() const {
    return bids_.empty() ? 0.0 : bids_.rbegin()->second;
}

Quantity LimitOrderBook::bestAskSize() const {
    return asks_.empty() ? 0.0 : asks_.begin()->second;
}

Quantity LimitOrderBook::volumeAt(Side side, Price price) const {
    const auto& m = levelMap(bids_, asks_, side);
    auto it = m.find(price);
    return (it == m.end()) ? 0.0 : it->second;
}

bool LimitOrderBook::empty() const {
    return bids_.empty() && asks_.empty();
}

size_t LimitOrderBook::bidLevels() const { return bids_.size(); }
size_t LimitOrderBook::askLevels() const { return asks_.size(); }

void LimitOrderBook::printSnapshot(std::ostream& os, size_t depth) const {
    os << "  BID                 ASK\n";
    auto bidIt = bids_.rbegin();
    auto askIt = asks_.begin();
    for (size_t i = 0; i < depth; ++i) {
        if (bidIt != bids_.rend()) {
            os << "  " << bidIt->first << " x " << bidIt->second;
            ++bidIt;
        } else {
            os << "  -";
        }
        os << "    ";
        if (askIt != asks_.end()) {
            os << askIt->first << " x " << askIt->second;
            ++askIt;
        } else {
            os << "-";
        }
        os << '\n';
    }
}

} // namespace cmf
