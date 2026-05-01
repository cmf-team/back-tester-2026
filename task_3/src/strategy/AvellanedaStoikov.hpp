#pragma once
#include "IStrategy.hpp"
#include <cmath>
#include <deque>

// ── Avellaneda-Stoikov (2008) ──────────────────────────────────────────────
//
// Модель маркет-мейкера с управлением inventory.
//
// Ключевые формулы:
//
//   Резервная цена (куда смещаем котировки):
//     r = s - q * γ * σ² * (T - t)
//
//   Оптимальный спред:
//     δ = γ * σ² * (T - t) + (2/γ) * ln(1 + γ/κ)
//
//   Bid = r - δ/2
//   Ask = r + δ/2
//
// Параметры:
//   γ (gamma) — неприятие риска [0.01 .. 0.5]
//   κ (kappa) — интенсивность потока ордеров [0.1 .. 10]
//   σ (sigma) — волатильность (вычисляется из данных)
//   T         — горизонт торговли (в секундах)
//   q_max     — максимальная позиция (ограничение)

struct ASConfig
{
    double gamma = 0.1;        // неприятие риска
    double kappa = 1.5;        // интенсивность потока
    double T = 3600.0;         // горизонт в секундах (1 час)
    double q_max = 5000.0;     // макс позиция
    double order_size = 100.0; // размер ордера
    int vol_window = 50;       // окно для расчёта волатильности
};

class AvellanedaStoikov : public IStrategy
{
  public:
    explicit AvellanedaStoikov(ASConfig cfg = {}) : cfg_(cfg) {}

    void on_book(const BookSnapshot& snap,
                 OrderManager& om,
                 Metrics& metrics) override
    {

        double s = snap.mid();
        if (s <= 0)
            return;

        // Обновляем волатильность
        update_vol(s);
        if (sigma_ <= 0)
            return;

        // Время: нормализуем в [0, T]
        if (t_start_ == 0)
            t_start_ = snap.timestamp;
        double elapsed = (snap.timestamp - t_start_) / 1e9; // наносек → сек
        double t_remaining = std::max(cfg_.T - elapsed, 1.0);

        double q = metrics.inventory;

        // Ограничиваем позицию
        double q_clamped = std::max(-cfg_.q_max,
                                    std::min(cfg_.q_max, q));

        // sigma_ это std log-returns — переводим в абсолютные единицы цены
        // и ограничиваем разумным диапазоном
        double sigma_p = sigma_ * s;
        sigma_p = std::min(sigma_p, s * 0.005);     // макс 0.5% от цены
        sigma_p = std::max(sigma_p, snap.spread()); // мин = 1 тик

        // Резервная цена
        double r = s - q_clamped * cfg_.gamma * sigma_p * sigma_p * t_remaining;

        // Оптимальный спред
        double delta = cfg_.gamma * sigma_p * sigma_p * t_remaining + (2.0 / cfg_.gamma) * std::log(1.0 + cfg_.gamma / cfg_.kappa);

        // Спред: минимум 1 тик, максимум 0.1% от цены
        delta = std::max(delta, snap.spread());
        delta = std::min(delta, s * 0.001);

        double bid_price = r - delta / 2.0;
        double ask_price = r + delta / 2.0;

        // Inventory skew: если позиция большая — сдвигаем котировки
        // чтобы стимулировать разгрузку
        double inv_ratio = q_clamped / cfg_.q_max;
        if (inv_ratio > 0.5)
        {
            // Длинная позиция — снижаем ask чтобы быстрее продать
            ask_price -= delta * 0.3 * inv_ratio;
            bid_price -= delta * 0.5 * inv_ratio;
        }
        else if (inv_ratio < -0.5)
        {
            // Короткая позиция — поднимаем bid
            bid_price += delta * 0.3 * std::abs(inv_ratio);
            ask_price += delta * 0.5 * std::abs(inv_ratio);
        }

        // Не ставим если пересекает рынок
        if (bid_price >= snap.best_ask())
            bid_price = snap.best_ask() - snap.spread() * 0.1;
        if (ask_price <= snap.best_bid())
            ask_price = snap.best_bid() + snap.spread() * 0.1;

        om.cancel_all();
        om.place(bid_price, ask_price, cfg_.order_size);

        last_mid_ = s;
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

    const char* name() const override { return "Avellaneda-Stoikov (2008)"; }

    double sigma() const { return sigma_; }

  private:
    void update_vol(double mid)
    {
        mid_history_.push_back(mid);
        if ((int)mid_history_.size() > cfg_.vol_window)
            mid_history_.pop_front();

        if ((int)mid_history_.size() < 2)
            return;

        // Волатильность как стандартное отклонение log-returns
        std::vector<double> returns;
        for (size_t i = 1; i < mid_history_.size(); ++i)
        {
            double r = std::log(mid_history_[i] / mid_history_[i - 1]);
            returns.push_back(r);
        }
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
    std::deque<double> mid_history_;
    double sigma_ = 0.0;
    double last_mid_ = 0.0;
    uint64_t t_start_ = 0;
};
