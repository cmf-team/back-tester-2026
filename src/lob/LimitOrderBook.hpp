#pragma once

#include "OrderEntry.hpp"
#include "PriceLevel.hpp"
#include "common/MarketDataEvent.hpp"
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace cmf {

class LimitOrderBook {
public:
    LimitOrderBook() = default;
    ~LimitOrderBook();

    LimitOrderBook(const LimitOrderBook&)            = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;
    LimitOrderBook(LimitOrderBook&&)                 = default;
    LimitOrderBook& operator=(LimitOrderBook&&)      = default;

    // Route the event to the correct handler based on action.
    // Events where flags::should_skip is true are ignored.
    void apply(const MarketDataEvent& ev);

    int64_t best_bid_price() const { return best_bid ? best_bid->price : 0; }
    int64_t best_ask_price() const { return best_ask ? best_ask->price : 0; }

    // Total resting quantity at a price level (returns 0 if no such level).
    int64_t volume_at(int64_t scaled_price) const;

    // Print the top 'depth' bid and ask levels to stdout.
    void print_snapshot(int depth) const;

    void clear();

    static int64_t scale_price(double p) {
        return static_cast<int64_t>(std::round(p * 1e9));
    }

    static double unscale_price(int64_t sp) {
        return static_cast<double>(sp) / 1e9;
    }

private:
    PriceLevel* bid_tree = nullptr;
    PriceLevel* ask_tree = nullptr;
    PriceLevel* best_bid = nullptr;  // max of bid_tree — O(1) access
    PriceLevel* best_ask = nullptr;  // min of ask_tree — O(1) access

    std::unordered_map<OrderId,  OrderEntry*> orders_;  // O(1) cancel by order_id
    std::unordered_map<int64_t,  PriceLevel*> levels_;  // O(1) add to existing level

    void add_order(const MarketDataEvent& ev);
    void cancel_order(const MarketDataEvent& ev);
    void modify_order(const MarketDataEvent& ev);

    // Remove an empty price level from the tree and free it.
    void remove_level(PriceLevel* lv, side::Side s);

    // Free the subtree rooted at n (does not touch orders_ or levels_).
    static void free_tree(PriceLevel* n);
};

} // namespace cmf
