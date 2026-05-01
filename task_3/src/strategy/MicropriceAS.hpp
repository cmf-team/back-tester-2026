#pragma once
#include "AvellanedaStoikov.hpp"

// ── Microprice + Avellaneda-Stoikov (2018) ─────────────────────────────────
//
// Stoikov (2018) предлагает использовать microprice вместо mid-price
// как лучшую оценку "справедливой цены" следующей сделки.
//
// Microprice формула:
//   P* = ask * W_bid + bid * W_ask
//   где W_bid = V_bid / (V_bid + V_ask)
//       W_ask = V_ask / (V_bid + V_ask)
//
// Смысл: если на bid'е больше объёма — цена скорее пойдёт вверх,
// значит "справедливая" цена ближе к ask.
//
// Улучшение AS: используем microprice как s вместо mid-price.
// Это делает котировки более точными — мы лучше оцениваем
// направление рынка и сдвигаем спред соответственно.

inline double compute_microprice(const BookSnapshot& snap)
{
    double bid_v = snap.bids.empty() ? 0.0 : snap.bids[0].amount;
    double ask_v = snap.asks.empty() ? 0.0 : snap.asks[0].amount;
    double total = bid_v + ask_v;

    if (total <= 0)
        return snap.mid();

    double bid_p = snap.best_bid();
    double ask_p = snap.best_ask();

    // P* = ask * (bid_vol/total) + bid * (ask_vol/total)
    return ask_p * (bid_v / total) + bid_p * (ask_v / total);
}

// Расширенная версия microprice с несколькими уровнями стакана
// Взвешивает по всем доступным уровням
inline double compute_microprice_deep(const BookSnapshot& snap, int levels = 5)
{
    double bid_total = 0.0, ask_total = 0.0;
    double bid_vwap = 0.0, ask_vwap = 0.0;

    int n = std::min(levels, (int)std::min(snap.bids.size(), snap.asks.size()));
    for (int i = 0; i < n; ++i)
    {
        bid_total += snap.bids[i].amount;
        ask_total += snap.asks[i].amount;
        bid_vwap += snap.bids[i].price * snap.bids[i].amount;
        ask_vwap += snap.asks[i].price * snap.asks[i].amount;
    }

    if (bid_total <= 0 || ask_total <= 0)
        return snap.mid();

    bid_vwap /= bid_total;
    ask_vwap /= ask_total;
    double total = bid_total + ask_total;

    return ask_vwap * (bid_total / total) + bid_vwap * (ask_total / total);
}

// ── MicropriceAS стратегия ─────────────────────────────────────────────────
class MicropriceAS : public IStrategy
{
  public:
    explicit MicropriceAS(ASConfig cfg = {}) : cfg_(cfg) {}

    void on_book(const BookSnapshot& snap,
                 OrderManager& om,
                 Metrics& metrics) override
    {

        // Используем microprice вместо mid
        double mp = compute_microprice_deep(snap, 5);
        double s = snap.mid();
        if (mp <= 0 || s <= 0)
            return;

        update_vol(mp);
        if (sigma_ <= 0)
            return;

        if (t_start_ == 0)
            t_start_ = snap.timestamp;
        double elapsed = (snap.timestamp - t_start_) / 1e9;
        double t_remaining = std::max(cfg_.T - elapsed, 1.0);

        double q = metrics.inventory;
        double q_clamped = std::max(-cfg_.q_max,
                                    std::min(cfg_.q_max, q));

        double sigma_p = sigma_ * mp;
        sigma_p = std::min(sigma_p, mp * 0.005);
        sigma_p = std::max(sigma_p, snap.spread());

        // Резервная цена на основе microprice
        double r = mp - q_clamped * cfg_.gamma * sigma_p * sigma_p * t_remaining;

        // Спред
        double delta = cfg_.gamma * sigma_p * sigma_p * t_remaining + (2.0 / cfg_.gamma) * std::log(1.0 + cfg_.gamma / cfg_.kappa);

        delta = std::max(delta, snap.spread());
        delta = std::min(delta, mp * 0.001);

        double bid_price = r - delta / 2.0;
        double ask_price = r + delta / 2.0;

        // Inventory skew
        double inv_ratio = q_clamped / cfg_.q_max;
        if (inv_ratio > 0.5)
        {
            ask_price -= delta * 0.3 * inv_ratio;
            bid_price -= delta * 0.5 * inv_ratio;
        }
        else if (inv_ratio < -0.5)
        {
            bid_price += delta * 0.3 * std::abs(inv_ratio);
            ask_price += delta * 0.5 * std::abs(inv_ratio);
        }

        // Защита от пересечения рынка
        if (bid_price >= snap.best_ask())
            bid_price = snap.best_ask() - snap.spread() * 0.1;
        if (ask_price <= snap.best_bid())
            ask_price = snap.best_bid() + snap.spread() * 0.1;

        om.cancel_all();
        om.place(bid_price, ask_price, cfg_.order_size);

        last_mid_ = s;
        last_mp_ = mp;
    }

    void on_trade(const Trade& trade,
                  OrderManager& om,
                  Metrics& metrics) override
    {
        if (om.check_bid_fill(trade.price, trade.is_sell))
        {
            Fill f{trade.timestamp, true,
                   om.bid_order->price, om.bid_order->amount};
            metrics.on_fill(f, last_mid_);
            om.fill_bid();
        }
        if (om.check_ask_fill(trade.price, trade.is_sell))
        {
            Fill f{trade.timestamp, false,
                   om.ask_order->price, om.ask_order->amount};
            metrics.on_fill(f, last_mid_);
            om.fill_ask();
        }
    }

    const char* name() const override
    {
        return "Microprice + Avellaneda-Stoikov (2018)";
    }

    double last_microprice() const { return last_mp_; }
    double sigma() const { return sigma_; }

  private:
    void update_vol(double mp)
    {
        mp_history_.push_back(mp);
        if ((int)mp_history_.size() > cfg_.vol_window)
            mp_history_.pop_front();
        if ((int)mp_history_.size() < 2)
            return;

        std::vector<double> returns;
        for (size_t i = 1; i < mp_history_.size(); ++i)
            returns.push_back(std::log(mp_history_[i] / mp_history_[i - 1]));

        double mean = 0.0;
        for (double r : returns)
            mean += r;
        mean /= returns.size();

        double var = 0.0;
        for (double r : returns)
            var += (r - mean) * (r - mean);
        var /= returns.size();

        sigma_ = std::sqrt(var);
    }

    ASConfig cfg_;
    std::deque<double> mp_history_;
    double sigma_ = 0.0;
    double last_mid_ = 0.0;
    double last_mp_ = 0.0;
    uint64_t t_start_ = 0;
};
