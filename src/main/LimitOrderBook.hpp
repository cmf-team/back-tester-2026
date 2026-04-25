#pragma once

#include "common/MarketDataEvent.hpp"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <unordered_map>

namespace cmf {

class LimitOrderBook {
public:
    struct Bbo {
        int64_t bid_px = kInvalidPrice;
        int64_t bid_qty = 0;
        int64_t ask_px = kInvalidPrice;
        int64_t ask_qty = 0;
    };

    struct OrderState {
        uint32_t instrument_id = 0;
        int64_t price_scaled = kInvalidPrice;
        int64_t size = 0;
        char side = 'N';
    };

    static constexpr int64_t kPriceScale = 1'000'000'000LL;
    static constexpr int64_t kInvalidPrice = std::numeric_limits<int64_t>::min();

    static int64_t to_scaled_price(double px) noexcept {
        if (px != px) return kInvalidPrice;
        const double scaled = px * static_cast<double>(kPriceScale);
        return static_cast<int64_t>(scaled + (scaled >= 0.0 ? 0.5 : -0.5));
    }

    static double to_double_price(int64_t px_scaled) noexcept {
        if (px_scaled == kInvalidPrice) return std::numeric_limits<double>::quiet_NaN();
        return static_cast<double>(px_scaled) / static_cast<double>(kPriceScale);
    }

    void apply_add(const OrderState& ord) {
        if (ord.size <= 0 || ord.price_scaled == kInvalidPrice) return;
        adjust_level(ord.side, ord.price_scaled, ord.size);
    }

    void apply_cancel(const OrderState& ord, int64_t canceled) {
        if (canceled <= 0) return;
        adjust_level(ord.side, ord.price_scaled, -std::min(canceled, ord.size));
    }

    void apply_trade_or_fill(const OrderState& ord, int64_t traded) {
        apply_cancel(ord, traded);
    }

    void apply_modify(const OrderState& old_ord, const OrderState& new_ord) {
        if (old_ord.size > 0 && old_ord.price_scaled != kInvalidPrice) {
            adjust_level(old_ord.side, old_ord.price_scaled, -old_ord.size);
        }
        if (new_ord.size > 0 && new_ord.price_scaled != kInvalidPrice) {
            adjust_level(new_ord.side, new_ord.price_scaled, new_ord.size);
        }
    }

    int64_t volume_at_price(char side, int64_t px_scaled) const {
        if (is_bid(side)) {
            const auto it = bids_.find(px_scaled);
            return it == bids_.end() ? 0 : it->second;
        }

        const auto it = asks_.find(px_scaled);
        return it == asks_.end() ? 0 : it->second;
    }

    Bbo best_bid_ask() const {
        Bbo out{};
        if (!bids_.empty()) {
            const auto it = bids_.rbegin();
            out.bid_px = it->first;
            out.bid_qty = it->second;
        }
        if (!asks_.empty()) {
            const auto it = asks_.begin();
            out.ask_px = it->first;
            out.ask_qty = it->second;
        }
        return out;
    }

    void print_snapshot(uint32_t instrument_id, int depth = 3) const {
        const Bbo bbo = best_bid_ask();
        std::printf(
            "LOB instrument=%u best_bid=%0.9f x %" PRId64 " | best_ask=%0.9f x %" PRId64 "\n",
            instrument_id,
            to_double_price(bbo.bid_px),
            bbo.bid_qty,
            to_double_price(bbo.ask_px),
            bbo.ask_qty
        );

        int lvl = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && lvl < depth; ++it, ++lvl) {
            std::printf(
                "  BID L%d: %0.9f x %" PRId64 "\n",
                lvl + 1,
                to_double_price(it->first),
                it->second
            );
        }
        lvl = 0;
        for (auto it = asks_.begin(); it != asks_.end() && lvl < depth; ++it, ++lvl) {
            std::printf(
                "  ASK L%d: %0.9f x %" PRId64 "\n",
                lvl + 1,
                to_double_price(it->first),
                it->second
            );
        }
    }

private:
    static bool is_bid(char side) noexcept {
        return side == 'B';
    }

    void adjust_level(char side, int64_t px_scaled, int64_t delta) {
        if (px_scaled == kInvalidPrice || delta == 0) return;

        auto& levels = is_bid(side) ? bids_ : asks_;
        auto it = levels.find(px_scaled);
        if (it == levels.end()) {
            if (delta > 0) {
                levels.emplace(px_scaled, delta);
            }
            return;
        }

        const int64_t next = it->second + delta;
        if (next <= 0) {
            levels.erase(it);
        } else {
            it->second = next;
        }
    }

    std::map<int64_t, int64_t> bids_;
    std::map<int64_t, int64_t> asks_;
};

using OrderStore = std::unordered_map<uint64_t, LimitOrderBook::OrderState>;
using BookStore = std::unordered_map<uint32_t, LimitOrderBook>;

} // namespace cmf
