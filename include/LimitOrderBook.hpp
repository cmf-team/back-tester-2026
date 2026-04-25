#pragma once

#include "MarketDataEvent.hpp"

#include <map>
#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <limits>

/**
 * @brief Level-2 Limit Order Book for a single instrument.
 *
 * Internally maintains:
 *  - A per-order map  (order_id → {price, size, side})  for O(1) Cancel/Modify
 *  - Bid levels       std::map<price, qty>  descending  (best bid = rbegin)
 *  - Ask levels       std::map<price, qty>  ascending   (best ask = begin)
 *
 * All prices are stored as int64_t fixed-point (×1e9).
 * Apply() handles: Add, Modify, Cancel, Clear, Trade, Fill.
 */
class LimitOrderBook {
public:
    // ── Per-order record ──────────────────────────────────────────────────────
    struct OrderInfo {
        int64_t  price = 0;
        uint32_t size  = 0;
        char     side  = 'N';
    };

    // ── Level-2 aggregated view ───────────────────────────────────────────────
    // Bids: descending price → best bid first
    using BidLevels = std::map<int64_t, int64_t, std::greater<int64_t>>;
    // Asks: ascending price → best ask first
    using AskLevels = std::map<int64_t, int64_t>;

    explicit LimitOrderBook(uint32_t instrument_id, std::string symbol = "")
        : instrument_id_(instrument_id), symbol_(std::move(symbol)) {}

    // ── Main entry point ──────────────────────────────────────────────────────

    void apply(const MarketDataEvent& evt) {
        ++total_events_;
        last_ts_ = evt.ts_recv;

        switch (evt.action) {
            case 'A': handle_add(evt);    break;
            case 'M': handle_modify(evt); break;
            case 'C': handle_cancel(evt); break;
            case 'R': handle_clear(evt);  break;
            case 'T': handle_trade(evt);  break;
            case 'F': handle_fill(evt);   break;
            default: break;
        }
    }

    // ── Accessors ─────────────────────────────────────────────────────────────

    [[nodiscard]] uint32_t instrument_id() const { return instrument_id_; }
    [[nodiscard]] const std::string& symbol() const { return symbol_; }
    [[nodiscard]] uint64_t total_events()    const { return total_events_; }

    [[nodiscard]] int64_t best_bid() const {
        if (bids_.empty()) return std::numeric_limits<int64_t>::min();
        return bids_.begin()->first;
    }

    [[nodiscard]] int64_t best_ask() const {
        if (asks_.empty()) return std::numeric_limits<int64_t>::max();
        return asks_.begin()->first;
    }

    [[nodiscard]] int64_t bid_size_at(int64_t price) const {
        auto it = bids_.find(price);
        return (it != bids_.end()) ? it->second : 0;
    }

    [[nodiscard]] int64_t ask_size_at(int64_t price) const {
        auto it = asks_.find(price);
        return (it != asks_.end()) ? it->second : 0;
    }

    [[nodiscard]] int64_t spread() const {
        if (bids_.empty() || asks_.empty()) return -1;
        return best_ask() - best_bid();
    }

    [[nodiscard]] bool has_order(uint64_t order_id) const {
        return orders_.count(order_id) > 0;
    }

    // ── Snapshot printing ─────────────────────────────────────────────────────

    void print_snapshot(int depth = 5, std::ostream& os = std::cout) const {
        os << "\n┌─────────────────────────────────────────────────┐\n";
        os << "│  LOB Snapshot  instrument=" << instrument_id_;
        if (!symbol_.empty())
            os << "  " << symbol_.substr(0, 20);
        os << "\n│  ts=" << last_ts_ << "  events=" << total_events_ << "\n";
        os << "├──────────────────────┬──────────────────────────┤\n";
        os << "│        ASK           │        BID               │\n";
        os << "├──────────────────────┼──────────────────────────┤\n";

        // Collect top-N ask levels (ascending → reverse for display)
        std::vector<std::pair<int64_t,int64_t>> ask_levels, bid_levels;
        int cnt = 0;
        for (auto& [p, q] : asks_) { ask_levels.push_back({p,q}); if (++cnt==depth) break; }
        cnt = 0;
        for (auto& [p, q] : bids_) { bid_levels.push_back({p,q}); if (++cnt==depth) break; }

        // Print asks top-to-bottom (worst ask first)
        for (int i = (int)ask_levels.size()-1; i >= 0; --i) {
            os << "│  " << std::fixed << std::setprecision(6)
               << std::setw(10) << ask_levels[i].first * 1e-9
               << "  sz=" << std::setw(6) << ask_levels[i].second
               << "  │                          │\n";
        }
        os << "├──────────────────────┼──────────────────────────┤\n";
        // Print bids
        for (auto& [p, q] : bid_levels) {
            os << "│                      │  "
               << std::fixed << std::setprecision(6)
               << std::setw(10) << p * 1e-9
               << "  sz=" << std::setw(6) << q << "  │\n";
        }
        os << "└──────────────────────┴──────────────────────────┘\n";

        if (!bids_.empty() && !asks_.empty()) {
            os << "  Best Bid: " << best_bid() * 1e-9
               << "  Best Ask: " << best_ask() * 1e-9
               << "  Spread: "   << spread()   * 1e-9 << "\n";
        } else {
            os << "  Book is one-sided or empty.\n";
        }
    }

    // ── Level maps (read-only) ────────────────────────────────────────────────
    const BidLevels& bids() const { return bids_; }
    const AskLevels& asks() const { return asks_; }

private:
    // ── Internal handlers ─────────────────────────────────────────────────────

    void handle_add(const MarketDataEvent& evt) {
        if (evt.is_undefined_price()) return;
        // Store order info
        orders_[evt.order_id] = {evt.price, evt.size, evt.side};
        add_to_level(evt.side, evt.price, evt.size);
    }

    void handle_modify(const MarketDataEvent& evt) {
        auto it = orders_.find(evt.order_id);
        if (it == orders_.end()) {
            // Unknown order — treat as new Add
            handle_add(evt);
            return;
        }
        OrderInfo& old = it->second;
        // Remove old contribution
        remove_from_level(old.side, old.price, old.size);
        // Update
        old.price = evt.price;
        old.size  = evt.size;
        // Add new contribution
        add_to_level(old.side, old.price, old.size);
    }

    void handle_cancel(const MarketDataEvent& evt) {
        auto it = orders_.find(evt.order_id);
        if (it == orders_.end()) return;
        OrderInfo& info = it->second;
        uint32_t cancel_size = (evt.size > 0) ? evt.size : info.size;
        remove_from_level(info.side, info.price, cancel_size);
        if (cancel_size >= info.size)
            orders_.erase(it);
        else
            info.size -= cancel_size;
    }

    void handle_clear(const MarketDataEvent& /*evt*/) {
        bids_.clear();
        asks_.clear();
        orders_.clear();
    }

    void handle_trade(const MarketDataEvent& /*evt*/) {
        // Trade does not affect resting book — just count
        ++trade_count_;
    }

    void handle_fill(const MarketDataEvent& evt) {
        // Resting order was filled
        auto it = orders_.find(evt.order_id);
        if (it == orders_.end()) return;
        OrderInfo& info = it->second;
        uint32_t fill_size = (evt.size > 0) ? evt.size : info.size;
        remove_from_level(info.side, info.price, fill_size);
        if (fill_size >= info.size)
            orders_.erase(it);
        else
            info.size -= fill_size;
    }

    // ── Level helpers ─────────────────────────────────────────────────────────

    void add_to_level(char side, int64_t price, int64_t qty) {
        if (side == 'B') bids_[price] += qty;
        else if (side == 'A') asks_[price] += qty;
    }

    void remove_from_level(char side, int64_t price, int64_t qty) {
        auto remove = [&](auto& map) {
            auto it = map.find(price);
            if (it == map.end()) return;
            it->second -= qty;
            if (it->second <= 0) map.erase(it);
        };
        if (side == 'B') remove(bids_);
        else if (side == 'A') remove(asks_);
    }

    // ── State ─────────────────────────────────────────────────────────────────
    uint32_t    instrument_id_;
    std::string symbol_;

    std::unordered_map<uint64_t, OrderInfo> orders_;  // order_id → info
    BidLevels bids_;
    AskLevels asks_;

    uint64_t total_events_ = 0;
    uint64_t trade_count_  = 0;
    uint64_t last_ts_      = 0;
};
