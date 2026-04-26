#pragma once

#include "common/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <span>

namespace cmf {

class LimitOrderBook {
public:
    using ScaledPrice = std::int64_t;
    using AggQty      = std::uint64_t;

    static constexpr double SCALE = 1e9;

    static ScaledPrice scale(double px) noexcept {
        return static_cast<ScaledPrice>(px * SCALE + (px >= 0.0 ? 0.5 : -0.5));
    }
    static double unscale(ScaledPrice p) noexcept {
        return static_cast<double>(p) / SCALE;
    }

    struct Level {
        double   price = 0.0;
        AggQty   qty   = 0;
    };

    explicit LimitOrderBook(uint32_t instrument_id = 0) noexcept
        : instrument_id_(instrument_id) {}

    uint32_t instrument_id() const noexcept { return instrument_id_; }

    void apply_add(char side, ScaledPrice px, AggQty qty) noexcept;
    void apply_cancel(char side, ScaledPrice px, AggQty qty) noexcept;
    void apply_fill(char side, ScaledPrice px, AggQty filled_qty) noexcept;
    void clear() noexcept;

    bool best_bid(double& px, AggQty& qty) const noexcept;
    bool best_ask(double& px, AggQty& qty) const noexcept;
    AggQty volume_at(char side, ScaledPrice px) const noexcept;
    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

    std::size_t bid_levels() const noexcept { return bids_.size(); }
    std::size_t ask_levels() const noexcept { return asks_.size(); }

    std::size_t snapshot_bids(std::span<Level> out) const noexcept;
    std::size_t snapshot_asks(std::span<Level> out) const noexcept;

private:
    uint32_t instrument_id_;
    std::map<ScaledPrice, AggQty, std::greater<>> bids_;
    std::map<ScaledPrice, AggQty>                 asks_;
};

} // namespace cmf
