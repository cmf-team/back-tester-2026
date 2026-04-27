#pragma once

#include "common/BasicTypes.hpp"

#include <iosfwd>
#include <map>

namespace cmf {

// L2 order book for a single instrument.
// Aggregated quantity per price level. Bids and asks live in separate maps
// because their "best price" semantics are opposite (highest bid, lowest ask).
class LimitOrderBook {
public:
    // mutators
    void addLiquidity(Side side, Price price, Quantity size);
    void removeLiquidity(Side side, Price price, Quantity size);
    void clearBook();

    // queries
    Price bestBid() const;
    Price bestAsk() const;
    Quantity bestBidSize() const;
    Quantity bestAskSize() const;
    Quantity volumeAt(Side side, Price price) const;

    bool empty() const;
    size_t bidLevels() const;
    size_t askLevels() const;

    void printSnapshot(std::ostream& os, size_t depth = 5) const;

private:
    // ascending order. best bid = rbegin (highest price), best ask = begin (lowest)
    std::map<Price, Quantity> bids_;
    std::map<Price, Quantity> asks_;
};

} // namespace cmf
