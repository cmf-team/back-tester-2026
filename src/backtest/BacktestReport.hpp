#pragma once

#include "backtest/BacktestMetrics.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace md {

struct BacktestReportInput {
    std::string strategy_name;
    std::uint64_t instrument_id{};
    std::size_t events_processed{};
    std::size_t orders_placed{};
    std::size_t orders_cancelled{};
    BacktestMetrics metrics;
    std::int64_t max_inventory{};
    long double average_inventory{};
    double wall_clock_seconds{};
};

struct BacktestReport {
    std::string strategy_name;
    std::uint64_t instrument_id{};
    std::size_t events_processed{};
    std::size_t orders_placed{};
    std::size_t orders_cancelled{};
    std::uint64_t fills{};
    std::int64_t final_inventory{};
    long double turnover{};
    long double cash{};
    long double mark_to_market_pnl{};
    std::int64_t max_inventory{};
    long double average_inventory{};
    double wall_clock_seconds{};
    double throughput_messages_per_second{};
};

[[nodiscard]] BacktestReport makeBacktestReport(const BacktestReportInput& input);

void printBacktestReport(const BacktestReport& report, std::ostream& out);
void printBacktestComparisonCsv(const std::vector<BacktestReport>& reports, std::ostream& out);

} // namespace md
