#include "LimitOrderBook.hpp"
#include <algorithm>
#include <iomanip>

// ── level helpers ──────────────────────────────────────────────────────────

void LimitOrderBook::increase_level(Side side, int64_t price, int64_t qty) {
    if (side == Side::Bid) bids_[price] += qty;
    else                   asks_[price] += qty;
}

void LimitOrderBook::decrease_level(Side side, int64_t price, int64_t qty) {
    if (side == Side::Bid) {
        auto it = bids_.find(price);
        if (it == bids_.end()) return;
        it->second -= qty;
        if (it->second <= 0) bids_.erase(it);
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) return;
        it->second -= qty;
        if (it->second <= 0) asks_.erase(it);
    }
}

// ── reset ──────────────────────────────────────────────────────────────────

void LimitOrderBook::reset() {
    orders_.clear();
    bids_.clear();
    asks_.clear();
}

// ── add ────────────────────────────────────────────────────────────────────

void LimitOrderBook::add_order(const MarketDataEvent& ev) {
    // F_MBP: aggregated level update — qty is the *full* qty at the level
    if (ev.flags & Flags::F_MBP) {
        if (ev.qty == 0) {
            if (ev.side == Side::Bid) bids_.erase(ev.price);
            else                      asks_.erase(ev.price);
        } else {
            if (ev.side == Side::Bid) bids_[ev.price] = ev.qty;
            else                      asks_[ev.price] = ev.qty;
        }
        return;
    }

    // Standard MBO: if order already exists remove old level contribution
    auto it = orders_.find(ev.order_id);
    if (it != orders_.end()) {
        decrease_level(it->second.side, it->second.price, it->second.qty);
        orders_.erase(it);
    }

    Order ord{ev.order_id, ev.instrument_id, ev.side, ev.price, ev.qty};
    orders_[ev.order_id] = ord;
    increase_level(ev.side, ev.price, ev.qty);
}

// ── cancel ─────────────────────────────────────────────────────────────────

void LimitOrderBook::cancel_order(const MarketDataEvent& ev) {
    auto it = orders_.find(ev.order_id);
    if (it == orders_.end()) return;

    Order& ord = it->second;
    int64_t reduce = (ev.qty > 0) ? std::min(ord.qty, ev.qty) : ord.qty;
    decrease_level(ord.side, ord.price, reduce);
    ord.qty -= reduce;
    if (ord.qty <= 0) orders_.erase(it);
}

// ── modify ─────────────────────────────────────────────────────────────────

void LimitOrderBook::modify_order(const MarketDataEvent& ev) {
    auto it = orders_.find(ev.order_id);
    if (it == orders_.end()) return;

    Order& ord = it->second;
    decrease_level(ord.side, ord.price, ord.qty);
    ord = {ev.order_id, ev.instrument_id, ev.side, ev.price, ev.qty};
    if (ord.qty > 0) increase_level(ord.side, ord.price, ord.qty);
    else             orders_.erase(it);
}

// ── apply ──────────────────────────────────────────────────────────────────

void LimitOrderBook::apply_event(const MarketDataEvent& ev) {
    switch (ev.type) {
        case EventType::Add:                   add_order(ev);    break;
        case EventType::Cancel:
        case EventType::Trade:
        case EventType::Fill:                  cancel_order(ev); break;
        case EventType::Modify:                modify_order(ev); break;
        case EventType::Reset:                 reset();          break;
    }
}

// ── queries ────────────────────────────────────────────────────────────────

std::optional<int64_t> LimitOrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<int64_t> LimitOrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

int64_t LimitOrderBook::volume_at_price(Side side, int64_t price) const {
    if (side == Side::Bid) {
        auto it = bids_.find(price);
        return it != bids_.end() ? it->second : 0;
    }
    auto it = asks_.find(price);
    return it != asks_.end() ? it->second : 0;
}

// ── print ──────────────────────────────────────────────────────────────────

void LimitOrderBook::print_snapshot(std::size_t depth) const {
    auto fmt_price = [](int64_t p) {
        return std::to_string(p / 1'000'000'000) + "."
             + std::to_string(std::abs(p % 1'000'000'000) / 100'000); // 4 dp approx
    };

    // Print asks in reverse (worst → best) so best ask is closest to spread
    std::vector<std::pair<int64_t,int64_t>> ask_levels(asks_.begin(), asks_.end());
    std::size_t ask_start = ask_levels.size() > depth ? ask_levels.size() - depth : 0;
    std::cout << "  ASK:\n";
    for (std::size_t i = ask_start; i < ask_levels.size(); ++i)
        std::cout << "    " << fmt_price(ask_levels[i].first)
                  << " x " << ask_levels[i].second << "\n";

    std::cout << "  -------- spread --------\n";

    std::cout << "  BID:\n";
    std::size_t n = 0;
    for (auto& [p, q] : bids_) {
        if (n++ >= depth) break;
        std::cout << "    " << fmt_price(p) << " x " << q << "\n";
    }
}
