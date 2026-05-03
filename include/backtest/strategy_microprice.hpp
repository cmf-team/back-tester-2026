#pragma once

#include "backtest/strategy_avellaneda.hpp"

#include <cstdint>
#include <deque>

namespace backtest {

class MicropriceAvellanedaStrategy final : public AvellanedaStoikovStrategy {
public:
    explicit MicropriceAvellanedaStrategy(AvellanedaStoikovConfig cfg = {},
                                          int64_t micro_window_us = 500'000) noexcept;

    void on_init() noexcept override;

protected:
    void post_update_reference(const MarketEvent& ev) noexcept override;
    [[nodiscard]] const char* summary_banner() const noexcept override;

private:
    struct TapeSlice {
        int64_t ts_us;
        int32_t buy_vol;
        int32_t sell_vol;
        int64_t buy_px_vol;
        int64_t sell_px_vol;
    };

    void prune_window_(int64_t now_us) noexcept;
    void push_slice_(const MarketEvent& ev) noexcept;
    [[nodiscard]] bool compute_microprice_(int64_t& out_ticks) const noexcept;

    int64_t micro_window_us_;

    std::deque<TapeSlice> window_;
    int64_t total_buy_vol_   = 0;
    int64_t total_sell_vol_  = 0;
    int64_t sum_buy_px_vol_  = 0;
    int64_t sum_sell_px_vol_ = 0;
};

}  // namespace backtest
