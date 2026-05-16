#include "backtest/BacktestReport.hpp"

#include <iomanip>
#include <ostream>

namespace md {

BacktestReport makeBacktestReport(const BacktestReportInput& input) {
    BacktestReport report;
    report.strategy_name = input.strategy_name;
    report.instrument_id = input.instrument_id;
    report.events_processed = input.events_processed;
    report.orders_placed = input.orders_placed;
    report.orders_cancelled = input.orders_cancelled;
    report.fills = input.metrics.fills;
    report.final_inventory = input.metrics.inventory;
    report.turnover = input.metrics.turnover;
    report.cash = input.metrics.cash;
    report.mark_to_market_pnl = input.metrics.mtm_pnl;
    report.max_inventory = input.max_inventory;
    report.average_inventory = input.average_inventory;
    report.wall_clock_seconds = input.wall_clock_seconds;
    report.throughput_messages_per_second = input.wall_clock_seconds > 0.0
        ? static_cast<double>(input.events_processed) / input.wall_clock_seconds
        : 0.0;
    return report;
}

void printBacktestReport(const BacktestReport& report, std::ostream& out) {
    out << "Backtest Report\n"
        << "strategy=" << report.strategy_name << '\n'
        << "instrument_id=" << report.instrument_id << '\n'
        << "events_processed=" << report.events_processed << '\n'
        << "orders_placed=" << report.orders_placed << '\n'
        << "orders_cancelled=" << report.orders_cancelled << '\n'
        << "fills=" << report.fills << '\n'
        << "final_inventory=" << report.final_inventory << '\n';

    out << std::fixed << std::setprecision(6)
        << "turnover=" << static_cast<double>(report.turnover) << '\n'
        << "cash=" << static_cast<double>(report.cash) << '\n'
        << "mark_to_market_pnl=" << static_cast<double>(report.mark_to_market_pnl) << '\n'
        << "max_inventory=" << report.max_inventory << '\n'
        << "average_inventory=" << static_cast<double>(report.average_inventory) << '\n'
        << "wall_clock_seconds=" << report.wall_clock_seconds << '\n'
        << "throughput_messages_per_second=" << report.throughput_messages_per_second << '\n';
    out.unsetf(std::ios::floatfield);
}

void printBacktestComparisonCsv(const std::vector<BacktestReport>& reports, std::ostream& out) {
    out << "Strategy,Events,Fills,Orders,Cancels,FinalInventory,Turnover,MTM_PnL,Seconds,Throughput\n";

    for (const auto& report : reports) {
        out << report.strategy_name << ','
            << report.events_processed << ','
            << report.fills << ','
            << report.orders_placed << ','
            << report.orders_cancelled << ','
            << report.final_inventory << ','
            << std::fixed << std::setprecision(6)
            << static_cast<double>(report.turnover) << ','
            << static_cast<double>(report.mark_to_market_pnl) << ','
            << report.wall_clock_seconds << ','
            << report.throughput_messages_per_second << '\n';
        out.unsetf(std::ios::floatfield);
    }
}

} // namespace md
