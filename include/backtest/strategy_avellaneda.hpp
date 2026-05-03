#pragma once

#include "backtest/market_event.hpp"
#include "backtest/order.hpp"
#include "backtest/execution_engine.hpp"
#include "backtest/trade_logger.hpp"

#include <cmath>
#include <cstdint>

namespace backtest {

struct AvellanedaStoikovConfig {
    int64_t initial_capital_ticks = 100'000'000'000;
    int64_t commission_bps        = 10;

    int32_t order_qty             = 100;
    double gamma                  = 0.05;
    double k                      = 1.5;
    double ewma_lambda            = 0.995;
    int64_t horizon_us            = 3'600'000'000;
    int32_t max_inventory         = 10'000;
    int64_t queue_penalty_ticks   = 0;
    double adverse_inventory_bps  = 0.0;
    double sigma_sq_floor         = 1.0;
    double tau_floor_sec          = 1e-6;

    bool enable_trade_logging     = false;
    bool verbose                  = false;
};

class AvellanedaStoikovStrategy {
public:
    explicit AvellanedaStoikovStrategy(AvellanedaStoikovConfig cfg = {}) noexcept;

    virtual void on_init() noexcept;
    virtual void on_event(const MarketEvent& ev) noexcept;
    virtual void on_finish() noexcept;

    [[nodiscard]] const AvellanedaStoikovConfig& config() const noexcept { return cfg_; }

    [[nodiscard]] int32_t inventory() const noexcept { return inventory_; }
    [[nodiscard]] int64_t cash_ticks() const noexcept { return cash_ticks_; }
    [[nodiscard]] int64_t turnover_notional_ticks() const noexcept { return turnover_notional_ticks_; }
    [[nodiscard]] int64_t quotes_replaced() const noexcept { return quotes_replaced_; }

protected:
    virtual void post_update_reference(const MarketEvent& ev) noexcept;

    [[nodiscard]] virtual const char* summary_banner() const noexcept {
        return "Avellaneda–Stoikov run";
    }

    AvellanedaStoikovConfig cfg_;
    ExecutionEngine engine_;
    TradeLogger trade_logger_;

    int64_t reference_ticks_      = 0;
    int64_t mid_ticks_            = 0;
    int64_t last_bid_ticks_       = 0;
    int64_t last_ask_ticks_       = 0;

    double sigma_sq_per_sec_      = 0.0;

private:
    void reset_session_anchor(const MarketEvent& ev) noexcept;
    void process_fills_(const MarketEvent& ev) noexcept;
    void update_market_state_(const MarketEvent& ev) noexcept;
    void refresh_quotes_(const MarketEvent& ev) noexcept;

    void apply_buy_fill_(const ExecutionReport& rep) noexcept;
    void apply_sell_fill_(const ExecutionReport& rep) noexcept;

    [[nodiscard]] double tau_sec_(int64_t ts_us) const noexcept;
    [[nodiscard]] bool compute_quotes_(int64_t ts_us, int64_t& bid_px, int64_t& ask_px) const noexcept;

    [[nodiscard]] static double clamp_gamma_(double gamma) noexcept;
    [[nodiscard]] static double clamp_k_(double k) noexcept;

    bool initialized_session_     = false;
    int64_t session_end_us_       = 0;

    int64_t cash_ticks_           = 0;
    int32_t inventory_            = 0;

    int64_t mid_ticks_prev_       = 0;
    int64_t last_ts_us_           = 0;

    Order bid_order_{};
    Order ask_order_{};
    bool bid_live_                = false;
    bool ask_live_                = false;

    int64_t turnover_notional_ticks_ = 0;
    int64_t quotes_replaced_         = 0;

    int64_t next_order_id_        = 1;
    int64_t next_trade_id_        = 1;

    int64_t last_mark_ticks_      = 0;
};

}  // namespace backtest
