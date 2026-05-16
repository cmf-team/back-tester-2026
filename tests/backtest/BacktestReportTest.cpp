#include "TestSupport.hpp"

#include "backtest/BacktestReport.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace md::test {
namespace {

BacktestReport reportFor(const std::string& strategy_name, std::size_t events = 100) {
    BacktestMetrics metrics;
    metrics.cash = 12.5L;
    metrics.turnover = 2500.0L;
    metrics.mtm_pnl = 7.5L;
    metrics.inventory = -2;
    metrics.fills = 3;

    return makeBacktestReport(BacktestReportInput{
        .strategy_name = strategy_name,
        .instrument_id = 442,
        .events_processed = events,
        .orders_placed = 9,
        .orders_cancelled = 6,
        .metrics = metrics,
        .max_inventory = 4,
        .average_inventory = 1.25L,
        .wall_clock_seconds = 0.5
    });
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream input{text};
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::vector<std::string> splitCsvColumns(const std::string& line) {
    std::vector<std::string> columns;
    std::istringstream input{line};
    std::string column;
    while (std::getline(input, column, ',')) {
        columns.push_back(column);
    }
    return columns;
}

} // namespace

void testBacktestReportContainsRequiredMetrics() {
    std::ostringstream out;

    printBacktestReport(reportFor("avellaneda_stoikov"), out);
    const auto text = out.str();

    requireContains(text, "Backtest Report", "backtest_report_contains_required_metrics: title");
    requireContains(text, "strategy=avellaneda_stoikov", "backtest_report_contains_required_metrics: strategy");
    requireContains(text, "instrument_id=442", "backtest_report_contains_required_metrics: instrument");
    requireContains(text, "events_processed=100", "backtest_report_contains_required_metrics: events");
    requireContains(text, "orders_placed=9", "backtest_report_contains_required_metrics: orders");
    requireContains(text, "orders_cancelled=6", "backtest_report_contains_required_metrics: cancels");
    requireContains(text, "fills=3", "backtest_report_contains_required_metrics: fills");
    requireContains(text, "final_inventory=-2", "backtest_report_contains_required_metrics: inventory");
    requireContains(text, "turnover=2500.000000", "backtest_report_contains_required_metrics: turnover");
    requireContains(text, "cash=12.500000", "backtest_report_contains_required_metrics: cash");
    requireContains(text, "mark_to_market_pnl=7.500000", "backtest_report_contains_required_metrics: mtm");
    requireContains(text, "max_inventory=4", "backtest_report_contains_required_metrics: max inventory");
    requireContains(text, "average_inventory=1.250000", "backtest_report_contains_required_metrics: avg inventory");
    requireContains(text, "wall_clock_seconds=0.500000", "backtest_report_contains_required_metrics: seconds");
    requireContains(
        text,
        "throughput_messages_per_second=200.000000",
        "backtest_report_contains_required_metrics: throughput"
    );
}

void testBacktestReportCsvIsParseable() {
    std::ostringstream out;
    printBacktestComparisonCsv({
        reportFor("fixed_quote", 100),
        reportFor("avellaneda_stoikov", 200)
    }, out);

    const auto lines = splitLines(out.str());
    require(lines.size() == 3, "backtest_report_csv_is_parseable: line count");
    require(
        lines.front() == "Strategy,Events,Fills,Orders,Cancels,FinalInventory,Turnover,MTM_PnL,Seconds,Throughput",
        "backtest_report_csv_is_parseable: header"
    );

    for (const auto& line : lines) {
        require(splitCsvColumns(line).size() == 10, "backtest_report_csv_is_parseable: column count");
    }

    const auto first_row = splitCsvColumns(lines[1]);
    require(first_row[0] == "fixed_quote", "backtest_report_csv_is_parseable: strategy");
    require(std::stoull(first_row[1]) == 100, "backtest_report_csv_is_parseable: events numeric");
    require(std::stoull(first_row[2]) == 3, "backtest_report_csv_is_parseable: fills numeric");
}

void testStrategyComparisonOutputsThreeRows() {
    std::ostringstream out;
    printBacktestComparisonCsv({
        reportFor("fixed_quote", 100),
        reportFor("avellaneda_stoikov", 100),
        reportFor("microprice_avellaneda_stoikov", 100)
    }, out);

    const auto text = out.str();
    const auto lines = splitLines(text);

    require(lines.size() == 4, "strategy_comparison_outputs_three_rows: rows including header");
    requireContains(text, "\nfixed_quote,", "strategy_comparison_outputs_three_rows: fixed quote row");
    requireContains(text, "\navellaneda_stoikov,", "strategy_comparison_outputs_three_rows: as row");
    requireContains(text, "\nmicroprice_avellaneda_stoikov,", "strategy_comparison_outputs_three_rows: microprice row");
}

} // namespace md::test
