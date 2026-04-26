#pragma once

#include "MarketDataEvent.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

class LimitOrderBook
{
  public:
    void applyEvent(const MarketDataEvent& event);

    // Returns {price, total_qty} at best level, or nullopt if the side is empty.
    std::optional<std::pair<int64_t, uint32_t>> bestBid() const;
    std::optional<std::pair<int64_t, uint32_t>> bestAsk() const;

    // Total quantity resting at a given fixed-point price (searches both sides).
    uint32_t volumeAtPrice(int64_t price) const;

    void printSnapshot(uint32_t instrument_id, int depth = 5) const;

    std::size_t orderCount() const { return orders_.size(); }
    std::size_t bidLevels() const { return bids_.size(); }
    std::size_t askLevels() const { return asks_.size(); }

  private:
    struct OrderEntry
    {
        Side side;
        int64_t price;
        uint32_t size;
    };

    // L3: one entry per live order
    std::unordered_map<uint64_t, OrderEntry> orders_;

    // L2 aggregated views
    // bids_ is highest-first so begin() == best bid
    std::map<int64_t, uint32_t, std::greater<int64_t>> bids_;
    // asks_ is lowest-first so begin() == best ask
    std::map<int64_t, uint32_t> asks_;

    void addToLevel(Side side, int64_t price, uint32_t qty);
    void subtractFromLevel(Side side, int64_t price, uint32_t qty);
};
