#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

struct Fill
{
    uint64_t timestamp;
    bool is_buy;
    double price;
    double amount;
};

class Metrics
{
  public:
    double cash = 0.0;      // денежная позиция
    double inventory = 0.0; // позиция в базовом активе
    double turnover = 0.0;  // суммарный объём
    int num_fills = 0;

    std::vector<double> pnl_history;

    void on_fill(const Fill& f, double mid_price)
    {
        if (f.is_buy)
        {
            cash -= f.price * f.amount;
            inventory += f.amount;
        }
        else
        {
            cash += f.price * f.amount;
            inventory -= f.amount;
        }
        turnover += f.price * f.amount;
        ++num_fills;
        pnl_history.push_back(pnl(mid_price));
    }

    // PnL = денежная позиция + рыночная стоимость inventory
    double pnl(double mid_price) const
    {
        return cash + inventory * mid_price;
    }

    double max_drawdown() const
    {
        if (pnl_history.empty())
            return 0.0;
        double peak = pnl_history[0];
        double dd = 0.0;
        for (double v : pnl_history)
        {
            peak = std::max(peak, v);
            dd = std::min(dd, v - peak);
        }
        return dd;
    }

    double sharpe() const
    {
        if (pnl_history.size() < 2)
            return 0.0;
        std::vector<double> returns;
        for (size_t i = 1; i < pnl_history.size(); ++i)
            returns.push_back(pnl_history[i] - pnl_history[i - 1]);
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double var = 0.0;
        for (double r : returns)
            var += (r - mean) * (r - mean);
        var /= returns.size();
        return var > 0 ? mean / std::sqrt(var) : 0.0;
    }

    void print(double final_mid) const
    {
        printf("\n=== PERFORMANCE METRICS ===\n");
        printf("  PnL        : %+.6f\n", pnl(final_mid));
        printf("  Inventory  : %+.2f\n", inventory);
        printf("  Turnover   : %.2f\n", turnover);
        printf("  Num fills  : %d\n", num_fills);
        printf("  Max DD     : %+.6f\n", max_drawdown());
        printf("  Sharpe     : %.4f\n", sharpe());
    }
};
