#include "backtest/strategy_avellaneda.hpp"

#include "backtest/pnl_calculator.hpp"
#include "backtest/report_generator.hpp"
#include "backtest/trade_record.hpp"

#include <algorithm>
#include <iostream>

namespace backtest {

AvellanedaStoikovStrategy::AvellanedaStoikovStrategy(AvellanedaStoikovConfig cfg) noexcept
    : cfg_(cfg), engine_(cfg.commission_bps) {}

void AvellanedaStoikovStrategy::on_init() noexcept {
    engine_.setCommissionBps(cfg_.commission_bps);
    cash_ticks_               = cfg_.initial_capital_ticks;
    inventory_                = 0;
    sigma_sq_per_sec_         = cfg_.sigma_sq_floor;

    mid_ticks_prev_           = 0;
    last_ts_us_               = 0;
    reference_ticks_          = 0;
    mid_ticks_                = 0;
    last_bid_ticks_           = 0;
    last_ask_ticks_           = 0;

    bid_live_                 = false;
    ask_live_                 = false;
    bid_order_                = {};
    ask_order_                = {};

    turnover_notional_ticks_  = 0;
    quotes_replaced_          = 0;

    next_order_id_            = 1;
    next_trade_id_            = 1;

    initialized_session_      = false;
    session_end_us_           = 0;
    last_mark_ticks_          = 0;

    trade_logger_.clear();
}

void AvellanedaStoikovStrategy::post_update_reference(const MarketEvent& /*ev*/) noexcept {
    reference_ticks_ = mid_ticks_;
}

void AvellanedaStoikovStrategy::reset_session_anchor(const MarketEvent& ev) noexcept {
    if (initialized_session_) {
        return;
    }
    session_end_us_      = ev.timestamp_us + cfg_.horizon_us;
    initialized_session_ = true;
}

double AvellanedaStoikovStrategy::tau_sec_(const int64_t ts_us) const noexcept {
    const double tau_raw =
        static_cast<double>(session_end_us_ - ts_us) / 1'000'000.0;
    return std::max(cfg_.tau_floor_sec, tau_raw);
}

double AvellanedaStoikovStrategy::clamp_gamma_(const double gamma) noexcept {
    constexpr double eps = 1e-9;
    return std::max(gamma, eps);
}

double AvellanedaStoikovStrategy::clamp_k_(const double k) noexcept {
    constexpr double eps = 1e-9;
    return std::max(k, eps);
}

bool AvellanedaStoikovStrategy::compute_quotes_(const int64_t ts_us, int64_t& bid_px,
                                                 int64_t& ask_px) const noexcept {
    if (reference_ticks_ <= 0 || mid_ticks_ <= 0) {
        return false;
    }

    const double gamma = clamp_gamma_(cfg_.gamma);
    const double k     = clamp_k_(cfg_.k);

    const double S = static_cast<double>(reference_ticks_);
    const double q = static_cast<double>(inventory_);

    const double sigma_sq =
        std::max(cfg_.sigma_sq_floor, sigma_sq_per_sec_);
    const double tau = tau_sec_(ts_us);

    const double reservation = S - (q * gamma * sigma_sq * tau);

    const double delta =
        (gamma * sigma_sq * tau) / 2.0 + (std::log(1.0 + gamma / k) / gamma);

    double bid_d = reservation - delta;
    double ask_d = reservation + delta;

    const double adverse_ticks =
        (static_cast<double>(mid_ticks_) * cfg_.adverse_inventory_bps) / 10'000.0;
    const double widen = static_cast<double>(cfg_.queue_penalty_ticks) + adverse_ticks;

    bid_d -= widen;
    ask_d += widen;

    bid_px = static_cast<int64_t>(std::floor(bid_d));
    ask_px = static_cast<int64_t>(std::ceil(ask_d));

    if (bid_px <= 0 || ask_px <= 0 || bid_px >= ask_px) {
        return false;
    }
    return true;
}

void AvellanedaStoikovStrategy::process_fills_(const MarketEvent& ev) noexcept {
    if (bid_live_ && bid_order_.isActive()) {
        const ExecutionReport rep = engine_.checkLimitOrder(bid_order_, ev);
        if (rep.status == OrderStatus::Filled) {
            bid_order_.status    = OrderStatus::Filled;
            bid_order_.filled_qty = rep.filled_qty;
            bid_live_            = false;
            apply_buy_fill_(rep);
        }
    }

    if (ask_live_ && ask_order_.isActive()) {
        const ExecutionReport rep = engine_.checkLimitOrder(ask_order_, ev);
        if (rep.status == OrderStatus::Filled) {
            ask_order_.status     = OrderStatus::Filled;
            ask_order_.filled_qty = rep.filled_qty;
            ask_live_             = false;
            apply_sell_fill_(rep);
        }
    }
}

void AvellanedaStoikovStrategy::apply_buy_fill_(const ExecutionReport& rep) noexcept {
    const int64_t notional = rep.avg_price * static_cast<int64_t>(rep.filled_qty);
    cash_ticks_ -= notional + rep.commission;

    inventory_ += rep.filled_qty;

    turnover_notional_ticks_ += notional;

    const int64_t mid_ref = (mid_ticks_ > 0) ? mid_ticks_ : reference_ticks_;

    if (cfg_.enable_trade_logging) {
        TradeRecord trade(next_trade_id_++, rep.order_id, rep.timestamp_us, OrderSide::Buy,
                          rep.filled_qty, rep.avg_price, rep.commission, 0, 0, mid_ref);
        trade_logger_.logTrade(trade);
    }

    if (cfg_.verbose) {
        std::cout << "[AS BUY ] px=" << rep.avg_price << " qty=" << rep.filled_qty
                  << " inv=" << inventory_ << '\n';
    }
}

void AvellanedaStoikovStrategy::apply_sell_fill_(const ExecutionReport& rep) noexcept {
    const int64_t notional = rep.avg_price * static_cast<int64_t>(rep.filled_qty);
    cash_ticks_ += notional - rep.commission;

    inventory_ -= rep.filled_qty;

    turnover_notional_ticks_ += notional;

    const int64_t mid_ref = (mid_ticks_ > 0) ? mid_ticks_ : reference_ticks_;

    if (cfg_.enable_trade_logging) {
        TradeRecord trade(next_trade_id_++, rep.order_id, rep.timestamp_us, OrderSide::Sell,
                          rep.filled_qty, rep.avg_price, rep.commission, 0, 0, mid_ref);
        trade_logger_.logTrade(trade);
    }

    if (cfg_.verbose) {
        std::cout << "[AS SELL] px=" << rep.avg_price << " qty=" << rep.filled_qty
                  << " inv=" << inventory_ << '\n';
    }
}

void AvellanedaStoikovStrategy::update_market_state_(const MarketEvent& ev) noexcept {
    if (ev.side == Side::Buy) {
        last_ask_ticks_ = ev.price_ticks;
    } else if (ev.side == Side::Sell) {
        last_bid_ticks_ = ev.price_ticks;
    }

    if (last_bid_ticks_ > 0 && last_ask_ticks_ > 0) {
        mid_ticks_ = (last_bid_ticks_ + last_ask_ticks_) / 2;
        if (last_bid_ticks_ > last_ask_ticks_) {
            // Crossed proxy book — fall back to last trade for stability.
            mid_ticks_ = ev.price_ticks;
        }
    } else {
        mid_ticks_ = ev.price_ticks;
    }

    if (last_ts_us_ > 0) {
        const double dt_us =
            static_cast<double>(ev.timestamp_us - last_ts_us_);
        if (dt_us > 0.0) {
            const double dt_sec = dt_us / 1'000'000.0;
            const double dm     = static_cast<double>(mid_ticks_ - mid_ticks_prev_);
            const double incr   = (dm * dm) / dt_sec;
            sigma_sq_per_sec_ =
                cfg_.ewma_lambda * sigma_sq_per_sec_ +
                (1.0 - cfg_.ewma_lambda) * incr;
        }
    }

    mid_ticks_prev_ = mid_ticks_;
    last_ts_us_     = ev.timestamp_us;
    last_mark_ticks_ = mid_ticks_;

    reference_ticks_ = mid_ticks_;
    post_update_reference(ev);

    if (reference_ticks_ <= 0) {
        reference_ticks_ = mid_ticks_;
    }
}

void AvellanedaStoikovStrategy::refresh_quotes_(const MarketEvent& ev) noexcept {
    int64_t bid_px = 0;
    int64_t ask_px = 0;
    if (!compute_quotes_(ev.timestamp_us, bid_px, ask_px)) {
        return;
    }

    const bool allow_bid = inventory_ < cfg_.max_inventory;
    const bool allow_ask = inventory_ > -cfg_.max_inventory;

    auto cancel_if_needed = [&](Order& ord, bool& live, const int64_t target_px,
                                const bool allow_side) noexcept {
        if (!allow_side) {
            if (live && ord.isActive()) {
                ord.status = OrderStatus::Cancelled;
                live       = false;
                ++quotes_replaced_;
            }
            return;
        }

        if (!live || !ord.isActive()) {
            return;
        }

        if (ord.price_ticks != target_px) {
            ord.status = OrderStatus::Cancelled;
            live       = false;
            ++quotes_replaced_;
        }
    };

    cancel_if_needed(bid_order_, bid_live_, bid_px, allow_bid);
    cancel_if_needed(ask_order_, ask_live_, ask_px, allow_ask);

    if (allow_bid && (!bid_live_ || !bid_order_.isActive())) {
        bid_order_ =
            Order::limit_buy(bid_px, cfg_.order_qty, next_order_id_++, ev.timestamp_us);
        bid_live_ = true;
        ++quotes_replaced_;
    }

    if (allow_ask && (!ask_live_ || !ask_order_.isActive())) {
        ask_order_ =
            Order::limit_sell(ask_px, cfg_.order_qty, next_order_id_++, ev.timestamp_us);
        ask_live_ = true;
        ++quotes_replaced_;
    }
}

void AvellanedaStoikovStrategy::on_event(const MarketEvent& ev) noexcept {
    reset_session_anchor(ev);
    process_fills_(ev);
    update_market_state_(ev);
    refresh_quotes_(ev);
}

void AvellanedaStoikovStrategy::on_finish() noexcept {
    const int64_t mark =
        (last_mark_ticks_ > 0) ? last_mark_ticks_ : reference_ticks_;

    const int64_t equity =
        cash_ticks_ + static_cast<int64_t>(inventory_) * mark;

    const int64_t mtm_pnl_ticks = equity - cfg_.initial_capital_ticks;

    if (cfg_.enable_trade_logging) {
        PnLCalculator calc;
        const PnLMetrics metrics =
            calc.calculate(trade_logger_, cfg_.initial_capital_ticks);

        ReportGenerator::printConsole(metrics, trade_logger_);
        ReportGenerator::exportCsv("output/as_trades.csv", trade_logger_);
        ReportGenerator::exportJson("output/as_report.json", metrics, trade_logger_);
        ReportGenerator::exportSummary("output/as_summary.txt", metrics);
    } else {
        std::cout << "\n=== " << summary_banner() << " ===\n";
        std::cout << "Inventory (lots):          " << inventory_ << '\n';
        std::cout << "Cash + inv*mark (ticks):   " << equity << '\n';
        std::cout << "Mark-to-market PnL (ticks):" << mtm_pnl_ticks << '\n';
        std::cout << "Turnover notional (ticks²):" << turnover_notional_ticks_ << '\n';
        std::cout << "Quotes touched:            " << quotes_replaced_ << '\n';
        std::cout << "Sigma² ticks²/s (last):    " << sigma_sq_per_sec_ << '\n';
        std::cout << "=============================\n";
    }
}

}  // namespace backtest
