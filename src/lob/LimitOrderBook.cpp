#include "lob/LimitOrderBook.hpp"

namespace cmf {

namespace {

template <class Map>
inline void add_qty(Map& m, LimitOrderBook::ScaledPrice px, LimitOrderBook::AggQty q) {
    auto it = m.find(px);
    if (it == m.end()) m.emplace(px, q);
    else it->second += q;
}

template <class Map>
inline void sub_qty(Map& m, LimitOrderBook::ScaledPrice px, LimitOrderBook::AggQty q) {
    auto it = m.find(px);
    if (it == m.end()) return;
    if (it->second <= q) m.erase(it);
    else it->second -= q;
}

} // namespace

void LimitOrderBook::apply_add(char side, ScaledPrice px, AggQty qty) noexcept {
    if (qty == 0) return;
    if (side == 'B') add_qty(bids_, px, qty);
    else if (side == 'A') add_qty(asks_, px, qty);
}

void LimitOrderBook::apply_cancel(char side, ScaledPrice px, AggQty qty) noexcept {
    if (qty == 0) return;
    if (side == 'B') sub_qty(bids_, px, qty);
    else if (side == 'A') sub_qty(asks_, px, qty);
}

void LimitOrderBook::apply_fill(char side, ScaledPrice px, AggQty filled_qty) noexcept {
    apply_cancel(side, px, filled_qty);
}

void LimitOrderBook::clear() noexcept {
    bids_.clear();
    asks_.clear();
}

bool LimitOrderBook::best_bid(double& px, AggQty& qty) const noexcept {
    if (bids_.empty()) return false;
    auto it = bids_.begin();
    px  = unscale(it->first);
    qty = it->second;
    return true;
}

bool LimitOrderBook::best_ask(double& px, AggQty& qty) const noexcept {
    if (asks_.empty()) return false;
    auto it = asks_.begin();
    px  = unscale(it->first);
    qty = it->second;
    return true;
}

LimitOrderBook::AggQty LimitOrderBook::volume_at(char side, ScaledPrice px) const noexcept {
    if (side == 'B') {
        auto it = bids_.find(px);
        return it == bids_.end() ? 0 : it->second;
    }
    if (side == 'A') {
        auto it = asks_.find(px);
        return it == asks_.end() ? 0 : it->second;
    }
    return 0;
}

std::size_t LimitOrderBook::snapshot_bids(std::span<Level> out) const noexcept {
    std::size_t n = 0;
    for (auto& [p, q] : bids_) {
        if (n >= out.size()) break;
        out[n++] = Level{unscale(p), q};
    }
    return n;
}

std::size_t LimitOrderBook::snapshot_asks(std::span<Level> out) const noexcept {
    std::size_t n = 0;
    for (auto& [p, q] : asks_) {
        if (n >= out.size()) break;
        out[n++] = Level{unscale(p), q};
    }
    return n;
}

} // namespace cmf
