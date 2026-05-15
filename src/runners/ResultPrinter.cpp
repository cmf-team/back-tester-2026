#include "runners/ResultPrinter.hpp"

#include "domain/MarketDataEvent.hpp"

#include <iomanip>
#include <iostream>

namespace md {
namespace {

template <typename Container>
void printEventList(const std::string& title, const Container& events, std::ostream& out) {
    out << title << '\n';

    if (events.empty()) {
        out << "<empty>\n";
        return;
    }

    for (const auto& event : events) {
        out << event << '\n';
    }
}

} // namespace

void printRunResult(const RunResult& result, std::ostream& out, bool verbose, std::size_t max_events_to_print) {
    out << "Summary\n";
    const bool is_standard = result.strategy_name == "standard";
    if (!result.strategy_name.empty() && !is_standard) {
        out << "strategy=" << result.strategy_name << '\n';
    }
    out << "total_messages_processed=" << result.summary.total_messages_processed << '\n';

    if (!is_standard || verbose) {
        out << "chronological_violations=" << result.summary.chronological_violations << '\n';
    }

    if (result.summary.first_timestamp.has_value()) {
        out << "first_timestamp=" << *result.summary.first_timestamp << '\n';
        out << "last_timestamp=" << *result.summary.last_timestamp << '\n';
    } else {
        out << "first_timestamp=<none>\n";
        out << "last_timestamp=<none>\n";
    }

    if (!is_standard && result.wall_clock_seconds > 0.0) {
        const double throughput = result.summary.total_messages_processed / result.wall_clock_seconds;
        out << std::fixed << std::setprecision(6);
        out << "wall_clock_seconds=" << result.wall_clock_seconds << '\n';
        out << "throughput_messages_per_second=" << throughput << '\n';
        out.unsetf(std::ios::floatfield);
    }

    if (max_events_to_print > 0) {
        printEventList("First 10 MarketDataEvent objects", result.summary.first_events, out);
        printEventList("Last 10 MarketDataEvent objects", result.summary.last_events, out);
    }

    if (verbose) {
        out << "Diagnostics\n"
            << "total_lines_read=" << result.diagnostics.total_lines_read << '\n';
    }
}

void printBenchmarkResults(const std::vector<RunResult>& results, std::ostream& out) {
    out << "Benchmark\n";
    out << "Strategy,Messages,ChronologicalViolations,WallClockSeconds,ThroughputMessagesPerSecond\n";

    for (const auto& result : results) {
        const double throughput = result.wall_clock_seconds > 0.0
            ? result.summary.total_messages_processed / result.wall_clock_seconds
            : 0.0;

        out << result.strategy_name << ','
            << result.summary.total_messages_processed << ','
            << result.summary.chronological_violations << ','
            << std::fixed << std::setprecision(6) << result.wall_clock_seconds << ','
            << std::fixed << std::setprecision(2) << throughput << '\n';

        out.unsetf(std::ios::floatfield);
    }
}

} // namespace md
