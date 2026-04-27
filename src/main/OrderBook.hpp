#pragma once

#include "MarketData.hpp"

#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_map>

struct Order {
    cmf::OrderId id{};
    cmf::Side side{};
    cmf::Price price{};
    cmf::Quantity size{};
};

class LimitOrderBook {
private:
    std::unordered_map<cmf::OrderId, Order> orders_;
    std::map<cmf::Price, cmf::Quantity> bids_;
    std::map<cmf::Price, cmf::Quantity> asks_;

    void remove_level(cmf::Side side, cmf::Price price, cmf::Quantity qty) {
        auto& book = (side == cmf::Side::Buy) ? bids_ : asks_;
        auto it = book.find(price);
        if (it == book.end()) return;

        if (it->second <= qty) {
            book.erase(it);
        } else {
            it->second -= qty;
        }
    }

    void add_level(cmf::Side side, cmf::Price price, cmf::Quantity qty) {
        auto& book = (side == cmf::Side::Buy) ? bids_ : asks_;
        book[price] += qty;
    }

public:
    void clear() {
        orders_.clear();
        bids_.clear();
        asks_.clear();
    }

    void apply(const MarketDataEvent& e) {
        switch (e.action) {
            case OrderAction::Add:
                on_add(e);
                break;
            case OrderAction::Modify:
                on_modify(e);
                break;
            case OrderAction::Cancel:
                on_cancel(e);
                break;
            case OrderAction::Trade:
            case OrderAction::Fill:
                on_fill(e);
                break;
            case OrderAction::Clear:
                clear();
                break;
            default:
                break;
        }
    }

    void on_add(const MarketDataEvent& e) {
        if (e.order_id == 0 || e.size <= 0 || e.price <= 0) return;

        Order o{e.order_id, e.side, e.price, e.size};
        orders_[e.order_id] = o;
        add_level(e.side, e.price, e.size);
    }

    void on_cancel(const MarketDataEvent& e) {
        auto it = orders_.find(e.order_id);
        if (it == orders_.end()) return;

        const Order o = it->second;
        remove_level(o.side, o.price, o.size);
        orders_.erase(it);
    }

    void on_modify(const MarketDataEvent& e) {
        auto it = orders_.find(e.order_id);
        if (it == orders_.end()) return;

        Order& o = it->second;

        remove_level(o.side, o.price, o.size);

        o.side = e.side;
        o.price = e.price;
        o.size = e.size;

        if (o.size > 0 && o.price > 0) {
            add_level(o.side, o.price, o.size);
        }
    }

    void on_fill(const MarketDataEvent& e) {
        auto it = orders_.find(e.order_id);
        if (it == orders_.end()) return;

        Order& o = it->second;
        const cmf::Quantity filled = std::min(o.size, e.size);

        remove_level(o.side, o.price, filled);
        o.size -= filled;

        if (o.size <= 0) {
            orders_.erase(it);
        }
    }

    cmf::Price best_bid() const {
        if (bids_.empty()) return 0;
        return bids_.rbegin()->first;
    }

    cmf::Price best_ask() const {
        if (asks_.empty()) return 0;
        return asks_.begin()->first;
    }

    cmf::Quantity volume_at_price(cmf::Side side, cmf::Price price) const {
        const auto& book = (side == cmf::Side::Buy) ? bids_ : asks_;
        auto it = book.find(price);
        return it == book.end() ? 0 : it->second;
    }

    void print_snapshot(std::size_t depth = 5) const {
        std::cout << "ASKS\n";
        std::size_t n = 0;
        for (auto it = asks_.begin(); it != asks_.end() && n < depth; ++it, ++n) {
            std::cout << it->first << " " << it->second << "\n";
        }

        std::cout << "BIDS\n";
        n = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && n < depth; ++it, ++n) {
            std::cout << it->first << " " << it->second << "\n";
        }
    }
};