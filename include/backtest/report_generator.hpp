#pragma once

#include "backtest/pnl_calculator.hpp"
#include "backtest/trade_logger.hpp"
#include <string>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <chrono>
#include <ctime>

namespace backtest {

// Генерирует отчёты в различных форматах: консоль, CSV, JSON, TXT.
class ReportGenerator {
public:

    static void printConsole(const PnLMetrics& metrics,
                             const TradeLogger& logger) noexcept {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "                         STRATEGY REPORT\n";
        std::cout << std::string(70, '=') << "\n\n";

        printSectionHeader("PnL SUMMARY");
        std::cout << std::fixed << std::setprecision(6);
        printMetric("Gross PnL",     metrics.grossPnlUsd(), "USD");
        printMetric("Commission",    metrics.commissionUsd(), "USD");
        printMetric("Net PnL",       metrics.netPnlUsd(), "USD");
        printMetric("Max Drawdown",  metrics.maxDrawdownUsd(), "USD");
        std::cout << "\n";

        printSectionHeader("TRADE STATISTICS");
        printMetric("Total Trades",  static_cast<double>(metrics.total_trades), "");
        printMetric("Winning",       static_cast<double>(metrics.winning_trades),
                    "(" + formatPercent(metrics.win_rate) + ")");
        printMetric("Losing",        static_cast<double>(metrics.losing_trades), "");
        printMetric("Breakeven",     static_cast<double>(metrics.breakeven_trades), "");
        printMetric("Avg Win",       metrics.avgWinUsd(), "USD");
        printMetric("Avg Loss",      metrics.avgLossUsd(), "USD");
        printMetric("Largest Win",   PnLMetrics::ticksToUsd(
                                        static_cast<int64_t>(metrics.largest_win_ticks)), "USD");
        printMetric("Largest Loss",  PnLMetrics::ticksToUsd(
                                        static_cast<int64_t>(metrics.largest_loss_ticks)), "USD");
        printMetric("Profit Factor", metrics.profit_factor, "");
        std::cout << "\n";

        printSectionHeader("RISK METRICS");
        printMetric("Sharpe Ratio",  metrics.sharpe_ratio, "");
        printMetric("Sortino Ratio", metrics.sortino_ratio, "");
        printMetric("Recovery Factor", metrics.recovery_factor, "");
        if (metrics.max_drawdown_ticks > 0 && metrics.net_pnl_ticks != 0) {
            double dd_to_pnl = static_cast<double>(metrics.max_drawdown_ticks) /
                               std::abs(static_cast<double>(metrics.net_pnl_ticks));
            printMetric("Max DD / PnL",  dd_to_pnl, "x");
        }
        std::cout << "\n";

        printSectionHeader("EXECUTION QUALITY");
        double avg_slippage = 0.0;
        size_t slippage_count = 0;
        logger.forEachTrade([&](const TradeRecord& trade) {
            avg_slippage += trade.slippageBps();
            ++slippage_count;
        });
        if (slippage_count > 0) {
            avg_slippage /= static_cast<double>(slippage_count);
            printMetric("Avg Slippage",  avg_slippage, "bps");
        }

        printMetric("Total Volume",  static_cast<double>(logger.totalVolume()), "lots");
        printMetric("Commission Rate", 0.10, "%");
        std::cout << "\n";

        printSectionHeader("PERFORMANCE SUMMARY");
        std::string verdict;
        if (metrics.net_pnl_ticks > 0 && metrics.sharpe_ratio > 1.0) {
            verdict = "PROFITABLE (Good risk-adjusted returns)";
        } else if (metrics.net_pnl_ticks > 0) {
            verdict = "PROFITABLE (But risk-adjusted returns need improvement)";
        } else if (metrics.net_pnl_ticks < 0 && metrics.win_rate > 0.4) {
            verdict = "UNPROFITABLE (High win rate but commission too high)";
        } else {
            verdict = "UNPROFITABLE (Strategy needs optimization)";
        }

        std::cout << "  Verdict: " << verdict << "\n";
        std::cout << "\n" << std::string(70, '=') << "\n";
    }

    static bool exportCsv(const std::string& filepath,
                                         const TradeLogger& logger) noexcept {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[ReportGenerator] Failed to open: " << filepath << "\n";
            return false;
        }

        file << "trade_id,order_id,timestamp_us,side,quantity,exec_price,"
             << "commission_ticks,pnl_ticks,expected_price,slippage_bps,"
             << "exec_price_usd,pnl_usd,commission_usd\n";

        logger.forEachTrade([&](const TradeRecord& trade) {
            file << trade.trade_id << ","
                 << trade.order_id << ","
                 << trade.timestamp_us << ","
                 << (trade.side == OrderSide::Buy ? "BUY" : "SELL") << ","
                 << trade.quantity << ","
                 << trade.exec_price << ","
                 << trade.commission << ","
                 << trade.pnl_ticks << ","
                 << trade.expected_price << ","
                 << std::fixed << std::setprecision(3) << trade.slippageBps() << ","
                 << std::fixed << std::setprecision(7) << trade.execPriceUsd() << ","
                 << std::fixed << std::setprecision(7) << trade.pnlUsd() << ","
                 << std::fixed << std::setprecision(7) << trade.commissionUsd() << "\n";
        });

        return true;
    }

    static bool exportJson(const std::string& filepath,
                                          const PnLMetrics& metrics,
                                          const TradeLogger& logger) noexcept {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[ReportGenerator] Failed to open: " << filepath << "\n";
            return false;
        }

        auto now = std::chrono::system_clock::now();
        auto time_now = std::chrono::system_clock::to_time_t(now);

        file << "{\n";

        file << "  \"metadata\": {\n";
        file << "    \"generated_at\": " << time_now << ",\n";
        file << "    \"tick_to_usd\": " << PnLMetrics::ticksToUsd(1) << "\n";
        file << "  },\n";

        file << "  \"summary\": {\n";
        file << "    \"gross_pnl_usd\": " << metrics.grossPnlUsd() << ",\n";
        file << "    \"net_pnl_usd\": " << metrics.netPnlUsd() << ",\n";
        file << "    \"commission_usd\": " << metrics.commissionUsd() << ",\n";
        file << "    \"max_drawdown_usd\": " << metrics.maxDrawdownUsd() << ",\n";
        file << "    \"sharpe_ratio\": " << metrics.sharpe_ratio << ",\n";
        file << "    \"sortino_ratio\": " << metrics.sortino_ratio << ",\n";
        file << "    \"win_rate\": " << metrics.win_rate << ",\n";
        file << "    \"profit_factor\": " << metrics.profit_factor << ",\n";
        file << "    \"recovery_factor\": " << metrics.recovery_factor << ",\n";
        file << "    \"total_trades\": " << metrics.total_trades << ",\n";
        file << "    \"winning_trades\": " << metrics.winning_trades << ",\n";
        file << "    \"losing_trades\": " << metrics.losing_trades << "\n";
        file << "  },\n";

        file << "  \"trades\": [\n";
        bool first = true;
        logger.forEachTrade([&](const TradeRecord& trade) {
            if (!first) file << ",\n";
            first = false;

            file << "    {\n";
            file << "      \"trade_id\": " << trade.trade_id << ",\n";
            file << "      \"order_id\": " << trade.order_id << ",\n";
            file << "      \"timestamp_us\": " << trade.timestamp_us << ",\n";
            file << "      \"side\": \"" << (trade.side == OrderSide::Buy ? "buy" : "sell") << "\",\n";
            file << "      \"quantity\": " << trade.quantity << ",\n";
            file << "      \"exec_price\": " << trade.exec_price << ",\n";
            file << "      \"exec_price_usd\": " << trade.execPriceUsd() << ",\n";
            file << "      \"commission_ticks\": " << trade.commission << ",\n";
            file << "      \"commission_usd\": " << trade.commissionUsd() << ",\n";
            file << "      \"pnl_ticks\": " << trade.pnl_ticks << ",\n";
            file << "      \"pnl_usd\": " << trade.pnlUsd() << ",\n";
            file << "      \"expected_price\": " << trade.expected_price << ",\n";
            file << "      \"slippage_bps\": " << std::fixed << std::setprecision(3) << trade.slippageBps() << "\n";
            file << "    }";
        });
        file << "\n  ]\n";

        file << "}\n";

        return true;
    }

    static bool exportSummary(const std::string& filepath,
                                             const PnLMetrics& metrics) noexcept {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[ReportGenerator] Failed to open: " << filepath << "\n";
            return false;
        }

        file << "=== Strategy Performance Summary ===\n\n";
        file << std::fixed << std::setprecision(6);
        file << "Net PnL (USD):        " << metrics.netPnlUsd() << "\n";
        file << "Gross PnL (USD):      " << metrics.grossPnlUsd() << "\n";
        file << "Commission (USD):     " << metrics.commissionUsd() << "\n";
        file << "Max Drawdown (USD):   " << metrics.maxDrawdownUsd() << "\n";
        file << "Sharpe Ratio:         " << metrics.sharpe_ratio << "\n";
        file << "Win Rate:             " << (metrics.win_rate * 100.0) << "%\n";
        file << "Profit Factor:        " << metrics.profit_factor << "\n";
        file << "Recovery Factor:      " << metrics.recovery_factor << "\n";
        file << "Total Trades:         " << metrics.total_trades << "\n";
        file << "Winning Trades:       " << metrics.winning_trades << "\n";
        file << "Losing Trades:        " << metrics.losing_trades << "\n";

        return true;
    }

private:
    static void printSectionHeader(const std::string& title) noexcept {
        std::cout << title << "\n";
        std::cout << std::string(40, '-') << "\n";
    }

    static void printMetric(const std::string& name, double value,
                            const std::string& unit) noexcept {
        std::cout << "  " << std::setw(18) << std::left << name << ": "
                  << std::setw(14) << std::right << value << " " << unit << "\n";
    }

    [[nodiscard]] static std::string formatPercent(double ratio) noexcept {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << (ratio * 100.0) << "%";
        return oss.str();
    }
};

}