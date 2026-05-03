#include "backtest/strategy_microprice.hpp"

#include <cmath>

namespace backtest {

MicropriceAvellanedaStrategy::MicropriceAvellanedaStrategy(AvellanedaStoikovConfig cfg,
                                                           const int64_t micro_window_us) noexcept
    : AvellanedaStoikovStrategy(cfg), micro_window_us_(micro_window_us) {}

void MicropriceAvellanedaStrategy::on_init() noexcept {
    AvellanedaStoikovStrategy::on_init();
    window_.clear();
    window_.shrink_to_fit();

    total_buy_vol_   = 0;
    total_sell_vol_  = 0;
    sum_buy_px_vol_  = 0;
    sum_sell_px_vol_ = 0;
}

const char* MicropriceAvellanedaStrategy::summary_banner() const noexcept {
    return "Avellaneda–Stoikov + microprice reference";
}

void MicropriceAvellanedaStrategy::prune_window_(const int64_t now_us) noexcept {
    while (!window_.empty()) {
        const TapeSlice& front = window_.front();
        if (now_us - front.ts_us <= micro_window_us_) {
            break;
        }

        total_buy_vol_ -= static_cast<int64_t>(front.buy_vol);
        total_sell_vol_ -= static_cast<int64_t>(front.sell_vol);
        sum_buy_px_vol_ -= front.buy_px_vol;
        sum_sell_px_vol_ -= front.sell_px_vol;

        window_.pop_front();
    }
}

void MicropriceAvellanedaStrategy::push_slice_(const MarketEvent& ev) noexcept {
    TapeSlice slice{};
    slice.ts_us = ev.timestamp_us;

    if (ev.side == Side::Buy) {
        slice.buy_vol    = ev.amount;
        slice.buy_px_vol = ev.price_ticks * static_cast<int64_t>(ev.amount);
    } else if (ev.side == Side::Sell) {
        slice.sell_vol    = ev.amount;
        slice.sell_px_vol = ev.price_ticks * static_cast<int64_t>(ev.amount);
    } else {
        return;
    }

    total_buy_vol_ += static_cast<int64_t>(slice.buy_vol);
    total_sell_vol_ += static_cast<int64_t>(slice.sell_vol);
    sum_buy_px_vol_ += slice.buy_px_vol;
    sum_sell_px_vol_ += slice.sell_px_vol;

    window_.push_back(slice);
}

bool MicropriceAvellanedaStrategy::compute_microprice_(int64_t& out_ticks) const noexcept {
    if (total_buy_vol_ <= 0 || total_sell_vol_ <= 0) {
        return false;
    }

    const double p_bid =
        static_cast<double>(sum_sell_px_vol_) / static_cast<double>(total_sell_vol_);
    const double p_ask =
        static_cast<double>(sum_buy_px_vol_) / static_cast<double>(total_buy_vol_);

    const double v_bid = static_cast<double>(total_sell_vol_);
    const double v_ask = static_cast<double>(total_buy_vol_);

    const double numer = (p_ask * v_bid) + (p_bid * v_ask);
    const double denom = v_bid + v_ask;

    if (!(denom > 0.0)) {
        return false;
    }

    const double micro = numer / denom;
    if (!(micro > 0.0)) {
        return false;
    }

    out_ticks = static_cast<int64_t>(std::llround(micro));
    return out_ticks > 0;
}

void MicropriceAvellanedaStrategy::post_update_reference(const MarketEvent& ev) noexcept {
    prune_window_(ev.timestamp_us);
    push_slice_(ev);

    int64_t micro_ticks = 0;
    if (compute_microprice_(micro_ticks)) {
        reference_ticks_ = micro_ticks;
    } else {
        reference_ticks_ = mid_ticks_;
    }
}

}  // namespace backtest
