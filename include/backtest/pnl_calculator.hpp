#pragma once

#include "backtest/trade_logger.hpp"
#include "backtest/trade_record.hpp"
#include <cmath>
#include <vector>

namespace backtest {

struct PnLMetrics {
    int64_t gross_pnl_ticks = 0;
    int64_t net_pnl_ticks = 0;
    int64_t total_commission_ticks = 0;
    int64_t gross_profit_ticks = 0;
    int64_t gross_loss_ticks = 0;
    int64_t max_drawdown_ticks = 0;

    size_t total_trades = 0;
    size_t winning_trades = 0;
    size_t losing_trades = 0;
    size_t breakeven_trades = 0;

    double win_rate = 0.0;
    double profit_factor = 0.0;
    double recovery_factor = 0.0;
    double avg_win_ticks = 0.0;
    double avg_loss_ticks = 0.0;
    double avg_trade_pnl_ticks = 0.0;

    double largest_win_ticks = 0.0;
    double largest_loss_ticks = 0.0;

    double sharpe_ratio = 0.0;
    double sortino_ratio = 0.0;

    [[nodiscard]] static constexpr double ticksToUsd(int64_t ticks) noexcept {
        return static_cast<double>(ticks) * (1.0 / 10'000'000.0);
    }

    [[nodiscard]] double grossPnlUsd() const noexcept {
        return ticksToUsd(gross_pnl_ticks);
    }

    [[nodiscard]] double netPnlUsd() const noexcept {
        return ticksToUsd(net_pnl_ticks);
    }

    [[nodiscard]] double commissionUsd() const noexcept {
        return ticksToUsd(total_commission_ticks);
    }

    [[nodiscard]] double maxDrawdownUsd() const noexcept {
        return ticksToUsd(max_drawdown_ticks);
    }

    [[nodiscard]] double avgWinUsd() const noexcept {
        return avg_win_ticks * (1.0 / 10'000'000.0);
    }

    [[nodiscard]] double avgLossUsd() const noexcept {
        return avg_loss_ticks * (1.0 / 10'000'000.0);
    }
};

class PnLCalculator {
public:
    [[nodiscard]] PnLMetrics calculate(const TradeLogger& logger,
                                        int64_t initial_capital_ticks) noexcept {
        PnLMetrics metrics;

        metrics.total_trades = logger.totalTrades();
        metrics.total_commission_ticks = logger.totalCommission();

        if (metrics.total_trades == 0) {
            return metrics;
        }

        std::vector<double> returns;
        returns.reserve(metrics.total_trades);

        int64_t cumulative_pnl = 0;
        int64_t peak_equity = initial_capital_ticks;

        logger.forEachTrade([&](const TradeRecord& trade) {
            if (trade.pnl_ticks > 0) {
                metrics.gross_profit_ticks += trade.pnl_ticks;
                ++metrics.winning_trades;
                metrics.largest_win_ticks = std::max(
                    metrics.largest_win_ticks,
                    static_cast<double>(trade.pnl_ticks)
                );
            } else if (trade.pnl_ticks < 0) {
                metrics.gross_loss_ticks -= trade.pnl_ticks;
                ++metrics.losing_trades;
                metrics.largest_loss_ticks = std::min(
                    metrics.largest_loss_ticks,
                    static_cast<double>(trade.pnl_ticks)
                );
            } else {
                ++metrics.breakeven_trades;
            }

            cumulative_pnl += trade.pnl_ticks;
            int64_t current_equity = initial_capital_ticks + cumulative_pnl;
            if (current_equity > peak_equity) {
                peak_equity = current_equity;
            } else {
                int64_t drawdown = peak_equity - current_equity;
                metrics.max_drawdown_ticks = std::max(metrics.max_drawdown_ticks, drawdown);
            }

            if (trade.entry_price > 0) {
                double return_bps = PnLMetrics::ticksToUsd(trade.pnl_ticks) * 10'000
                                    / PnLMetrics::ticksToUsd(trade.entry_price);
                returns.push_back(return_bps);
            }
        });

        metrics.gross_pnl_ticks = metrics.gross_profit_ticks - metrics.gross_loss_ticks;
        metrics.net_pnl_ticks = metrics.gross_pnl_ticks - metrics.total_commission_ticks;

        metrics.win_rate = static_cast<double>(metrics.winning_trades) /
                           static_cast<double>(metrics.total_trades);

        if (metrics.winning_trades > 0) {
            metrics.avg_win_ticks = static_cast<double>(metrics.gross_profit_ticks) /
                                    static_cast<double>(metrics.winning_trades);
        }
        if (metrics.losing_trades > 0) {
            metrics.avg_loss_ticks = -static_cast<double>(metrics.gross_loss_ticks) /
                                     static_cast<double>(metrics.losing_trades);
        }
        if (metrics.total_trades > 0) {
            metrics.avg_trade_pnl_ticks = static_cast<double>(metrics.net_pnl_ticks) /
                                          static_cast<double>(metrics.total_trades);
        }

        if (metrics.gross_loss_ticks > 0) {
            metrics.profit_factor = static_cast<double>(metrics.gross_profit_ticks) /
                                    static_cast<double>(metrics.gross_loss_ticks);
        } else if (metrics.gross_profit_ticks > 0) {
            metrics.profit_factor = 999.0;
        }

        if (metrics.max_drawdown_ticks > 0) {
            metrics.recovery_factor = static_cast<double>(metrics.net_pnl_ticks) /
                                      static_cast<double>(metrics.max_drawdown_ticks);
        }

        calculateSharpe(returns, metrics.sharpe_ratio);
        calculateSortino(returns, metrics.sortino_ratio);

        return metrics;
    }

private:
    void calculateSharpe(const std::vector<double>& returns,
                         double& sharpe_ratio) noexcept {
        if (returns.size() < 2) { sharpe_ratio = 0.0; return; }

        double mean = 0.0;
        for (double r : returns) mean += r;
        mean /= static_cast<double>(returns.size());

        double variance = 0.0;
        for (double r : returns) {
            double diff = r - mean;
            variance += diff * diff;
        }
        double std_dev = std::sqrt(variance / static_cast<double>(returns.size() - 1));

        if (std_dev > 0) {
            constexpr double ANNUALIZATION = 252.0 * 6.5 * 3600.0;
            sharpe_ratio = (mean / std_dev) * std::sqrt(ANNUALIZATION);
        }
    }

    void calculateSortino(const std::vector<double>& returns,
                          double& sortino_ratio) noexcept {
        if (returns.size() < 2) { sortino_ratio = 0.0; return; }

        double mean = 0.0;
        for (double r : returns) mean += r;
        mean /= static_cast<double>(returns.size());

        double downside_var = 0.0;
        size_t downside_count = 0;
        for (double r : returns) {
            if (r < 0) {
                downside_var += r * r;
                ++downside_count;
            }
        }

        if (downside_count > 0) {
            double downside_dev = std::sqrt(downside_var / static_cast<double>(downside_count));
            if (downside_dev > 0) {
                constexpr double ANNUALIZATION = 252.0 * 6.5 * 3600.0;
                sortino_ratio = (mean / downside_dev) * std::sqrt(ANNUALIZATION);
            }
        }
    }
};

}