#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "AvellanedaStoikov.hpp"
#include "DataReader.hpp"
#include "FullMicropriceAS.hpp"
#include "IStrategy.hpp"
#include "Metrics.hpp"
#include "MicropriceAS.hpp"
#include "OrderManager.hpp"

struct SimResult
{
    std::string strategy_name;
    double final_pnl;
    double inventory;
    double turnover;
    double max_drawdown;
    double sharpe;
    int num_fills;
};

SimResult run_simulation(IStrategy& strategy,
                         const std::vector<BookSnapshot>& lob,
                         const std::vector<Trade>& trades)
{
    OrderManager om;
    Metrics metrics;
    size_t li = 0, ti = 0;
    double last_mid = lob.empty() ? 0.0 : lob[0].mid();

    while (li < lob.size() || ti < trades.size())
    {
        bool process_lob = false;
        if (li < lob.size() && ti < trades.size())
            process_lob = lob[li].timestamp <= trades[ti].timestamp;
        else
            process_lob = li < lob.size();

        if (process_lob)
        {
            last_mid = lob[li].mid();
            strategy.on_book(lob[li], om, metrics);
            ++li;
        }
        else
        {
            strategy.on_trade(trades[ti], om, metrics);
            ++ti;
        }
    }

    return SimResult{
        strategy.name(),
        metrics.pnl(last_mid),
        metrics.inventory,
        metrics.turnover,
        metrics.max_drawdown(),
        metrics.sharpe(),
        metrics.num_fills};
}

void print_result(const SimResult& r)
{
    printf("\n=== %s ===\n", r.strategy_name.c_str());
    printf("  PnL          : %+.6f\n", r.final_pnl);
    printf("  Inventory    : %+.2f\n", r.inventory);
    printf("  Turnover     : %.2f\n", r.turnover);
    printf("  Num fills    : %d\n", r.num_fills);
    printf("  Max Drawdown : %+.6f\n", r.max_drawdown);
    printf("  Sharpe       : %.4f\n", r.sharpe);
}

int main(int, char**)
{
    std::string lob_path = "data/lob.csv";
    std::string trades_path = "data/trades.csv";
    std::string table_path = "data/microprice_table.csv";

    printf("=== HFT Backtester: Avellaneda-Stoikov ===\n\n");

    auto t0 = std::chrono::steady_clock::now();
    std::vector<BookSnapshot> lob;
    std::vector<Trade> trades;
    try
    {
        lob = read_lob(lob_path);
        trades = read_trades(trades_path);
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
    printf("Loaded %zu LOB snapshots and %zu trades in %.2fs\n\n",
           lob.size(), trades.size(),
           std::chrono::duration<double>(
               std::chrono::steady_clock::now() - t0)
               .count());

    // Параметры
    ASConfig cfg;
    cfg.gamma = 0.01;
    cfg.kappa = 1.5;
    cfg.T = 1.0;
    cfg.q_max = 10000000.0;
    cfg.order_size = 50000.0;
    cfg.vol_window = 50;

    printf("Parameters: gamma=%.4f  kappa=%.2f  T=%.1f  order_size=%.0f\n\n",
           cfg.gamma, cfg.kappa, cfg.T, cfg.order_size);

    // [1] AS (2008)
    printf("[1] Running Avellaneda-Stoikov (2008)...\n");
    AvellanedaStoikov as_strategy(cfg);
    auto t1 = std::chrono::steady_clock::now();
    auto r1 = run_simulation(as_strategy, lob, trades);
    printf("    Done in %.3fs\n", std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - t1)
                                      .count());
    print_result(r1);

    // [2] Weighted Microprice + AS (2018)
    printf("\n[2] Running Weighted Microprice + AS (2018)...\n");
    MicropriceAS mp_strategy(cfg);
    auto t2 = std::chrono::steady_clock::now();
    auto r2 = run_simulation(mp_strategy, lob, trades);
    printf("    Done in %.3fs\n", std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - t2)
                                      .count());
    print_result(r2);

    // [3] Full Microprice + AS (Stoikov 2018)
    printf("\n[3] Running Full Microprice AS (Stoikov 2018)...\n");
    FullMicropriceAS fmp_strategy(cfg, table_path);
    auto t3 = std::chrono::steady_clock::now();
    auto r3 = run_simulation(fmp_strategy, lob, trades);
    printf("    Done in %.3fs\n", std::chrono::duration<double>(
                                      std::chrono::steady_clock::now() - t3)
                                      .count());
    print_result(r3);

    // Сравнение
    double imp2 = r1.final_pnl != 0
                      ? (r2.final_pnl - r1.final_pnl) / std::abs(r1.final_pnl) * 100.0
                      : 0.0;
    double imp3 = r1.final_pnl != 0
                      ? (r3.final_pnl - r1.final_pnl) / std::abs(r1.final_pnl) * 100.0
                      : 0.0;

    printf("\n=== COMPARISON ===\n");
    printf("  AS (2008):                    PnL=%+.4f  Fills=%d\n",
           r1.final_pnl, r1.num_fills);
    printf("  Weighted Microprice AS (2018): PnL=%+.4f  Fills=%d  improvement=%+.1f%%\n",
           r2.final_pnl, r2.num_fills, imp2);
    printf("  Full Microprice AS (2018):     PnL=%+.4f  Fills=%d  improvement=%+.1f%%\n",
           r3.final_pnl, r3.num_fills, imp3);

    return 0;
}
