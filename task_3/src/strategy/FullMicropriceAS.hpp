#pragma once
#include "AvellanedaStoikov.hpp"
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

// ── Full Microprice (Stoikov 2018) + AS ────────────────────────────────────
//
// Uses precomputed G* table from compute_microprice.py
// P_micro = M + G*(I_bucket, S_bucket)
//
// This is the full implementation from Stoikov (2018) Section 3,
// as opposed to the simplified weighted mid-price approximation.

class FullMicropriceAS : public IStrategy
{
  public:
    explicit FullMicropriceAS(ASConfig cfg = {},
                              const std::string& table_path = "data/microprice_table.csv")
        : cfg_(cfg)
    {
        load_table(table_path);
    }

    void on_book(const BookSnapshot& snap,
                 OrderManager& om,
                 Metrics& metrics) override
    {

        double bid = snap.best_bid();
        double ask = snap.best_ask();
        double mid = snap.mid();
        double s = snap.spread();

        if (mid <= 0)
            return;

        // Compute imbalance
        double qb = snap.bids.empty() ? 0.0 : snap.bids[0].amount;
        double qa = snap.asks.empty() ? 0.0 : snap.asks[0].amount;
        double total = qb + qa;
        double I = total > 0 ? qb / total : 0.5;

        // Lookup G* from table
        double g_star = lookup_gstar(I, s);

        // Full microprice
        double mp = mid + g_star;

        // Clamp to bid-ask range
        mp = std::max(bid, std::min(ask, mp));

        update_vol(mp);
        if (sigma_ <= 0)
            return;

        if (t_start_ == 0)
            t_start_ = snap.timestamp;
        double elapsed = (snap.timestamp - t_start_) / 1e9;
        double t_remaining = std::max(cfg_.T - elapsed, 0.01);

        double q = metrics.inventory;
        double q_clamped = std::max(-cfg_.q_max,
                                    std::min(cfg_.q_max, q));

        double sigma_p = sigma_ * mp;
        sigma_p = std::min(sigma_p, mp * 0.005);
        sigma_p = std::max(sigma_p, s);

        double r = mp - q_clamped * cfg_.gamma * sigma_p * sigma_p * t_remaining;
        double delta = cfg_.gamma * sigma_p * sigma_p * t_remaining + (2.0 / cfg_.gamma) * std::log(1.0 + cfg_.gamma / cfg_.kappa);

        delta = std::max(delta, s);
        delta = std::min(delta, mp * 0.001);

        double bid_price = r - delta / 2.0;
        double ask_price = r + delta / 2.0;

        if (bid_price >= snap.best_ask())
            bid_price = snap.best_ask() - s * 0.1;
        if (ask_price <= snap.best_bid())
            ask_price = snap.best_bid() + s * 0.1;

        om.cancel_all();
        om.place(bid_price, ask_price, cfg_.order_size);
        last_mid_ = mid;
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
        return table_loaded_
                   ? "Full Microprice AS (Stoikov 2018)"
                   : "Full Microprice AS (table not found - fallback to mid)";
    }

    bool table_loaded() const { return table_loaded_; }

  private:
    void load_table(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open())
        {
            table_loaded_ = false;
            return;
        }

        std::string line;
        std::getline(f, line); // header

        while (std::getline(f, line))
        {
            if (line.empty())
                continue;
            std::stringstream ss(line);
            std::string tok;
            std::vector<std::string> vals;
            while (std::getline(ss, tok, ','))
                vals.push_back(tok);
            if (vals.size() < 4)
                continue;

            int i_bucket = std::stoi(vals[0]);
            int s_bucket = std::stoi(vals[1]);
            double g_star = std::stod(vals[3]);
            table_[{i_bucket, s_bucket}] = g_star;
        }

        table_loaded_ = !table_.empty();
        printf("  Loaded microprice table: %zu entries\n", table_.size());
    }

    double lookup_gstar(double I, double spread) const
    {
        if (!table_loaded_)
            return 0.0;

        // Discretize
        int i_bucket = std::min((int)(I * 10), 9);
        double tick = tick_size_ > 0 ? tick_size_ : spread;
        int s_bucket = 0;
        if (spread > tick * 1.5)
            s_bucket = 1;
        if (spread > tick * 2.5)
            s_bucket = 2;

        auto it = table_.find({i_bucket, s_bucket});
        if (it != table_.end())
            return it->second;
        return 0.0;
    }

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

        // Estimate tick size from spread history
        if (tick_size_ <= 0 && !mp_history_.empty())
        {
            tick_size_ = std::abs(mp_history_.back() - mp_history_.front()) / mp_history_.size();
        }
    }

    ASConfig cfg_;
    std::map<std::pair<int, int>, double> table_;
    bool table_loaded_ = false;
    std::deque<double> mp_history_;
    double sigma_ = 0.0;
    double last_mid_ = 0.0;
    double tick_size_ = 0.0;
    uint64_t t_start_ = 0;
};
