#pragma once
#include "MarketDataEvent.hpp"
#include "Order.hpp"
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <unordered_map>

class LimitOrderBook
{
  public:
    void apply_event(const MarketDataEvent& ev);
    void reset();

    std::optional<int64_t> best_bid() const;
    std::optional<int64_t> best_ask() const;
    int64_t volume_at_price(Side side, int64_t price) const;
    void print_snapshot(std::size_t depth = 5) const;

    std::size_t order_count() const { return orders_.size(); }
    bool empty() const { return bids_.empty() && asks_.empty(); }

  private:
    std::unordered_map<uint64_t, Order> orders_;
    std::map<int64_t, int64_t, std::greater<int64_t>> bids_; // best bid at begin()
    std::map<int64_t, int64_t> asks_;                        // best ask at begin()

    void add_order(const MarketDataEvent& ev);
    void cancel_order(const MarketDataEvent& ev);
    void modify_order(const MarketDataEvent& ev);

    void increase_level(Side side, int64_t price, int64_t qty);
    void decrease_level(Side side, int64_t price, int64_t qty);
};
