#include "runners/ResultPrinter.hpp"

#include "domain/MarketDataEvent.hpp"

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

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

std::string digestFingerprint(std::string_view digest) {
    constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL;
    constexpr std::uint64_t fnv_prime = 1099511628211ULL;

    std::uint64_t hash = fnv_offset_basis;
    for (const unsigned char value : digest) {
        hash ^= value;
        hash *= fnv_prime;
    }

    std::ostringstream out;
    out << "0x" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

} // namespace

void printRunResult(const RunResult& result, std::ostream& out, bool verbose, std::size_t max_events_to_print) {
    out << "Summary\n";
    const bool is_standard = result.strategy_name == "standard";
    if (!result.strategy_name.empty() && !is_standard) {
        out << "strategy=" << result.strategy_name << '\n';
    }
    out << "total_messages_processed=" << result.summary.total_messages_processed << '\n';
    out << "chronological_violations=" << result.summary.chronological_violations << '\n';

    if (result.summary.first_timestamp.has_value()) {
        out << "first_timestamp=" << *result.summary.first_timestamp << '\n';
        out << "last_timestamp=" << *result.summary.last_timestamp << '\n';
    } else {
        out << "first_timestamp=<none>\n";
        out << "last_timestamp=<none>\n";
    }

    if (result.wall_clock_seconds > 0.0) {
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

void printBenchmarkResults(const std::vector<BenchmarkResult>& results, std::ostream& out) {
    out << "Benchmark\n";
    out << "Strategy,InputFormat,Processor,Messages,ChronologicalViolations,UnresolvedEvents,"
        << "WallClockSeconds,ThroughputMessagesPerSecond\n";

    for (const auto& benchmark : results) {
        const auto& result = benchmark.result;
        const double throughput = result.wall_clock_seconds > 0.0
            ? result.summary.total_messages_processed / result.wall_clock_seconds
            : 0.0;

        out << result.strategy_name << ','
            << benchmark.input_format << ','
            << benchmark.processor << ','
            << result.summary.total_messages_processed << ','
            << result.summary.chronological_violations << ','
            << benchmark.unresolved_events << ','
            << std::fixed << std::setprecision(6) << result.wall_clock_seconds << ','
            << std::fixed << std::setprecision(2) << throughput << '\n';

        out.unsetf(std::ios::floatfield);
    }
}

void printLobBenchmarkResults(const std::vector<BenchmarkResult>& results, std::ostream& out) {
    out << "Benchmark LOB\n";
    out << "Strategy,InputFormat,Processor,Messages,ChronologicalViolations,UnresolvedEvents,"
        << "WallClockSeconds,ThroughputMessagesPerSecond,LobDigest\n";

    for (const auto& benchmark : results) {
        const auto& result = benchmark.result;
        const double throughput = result.wall_clock_seconds > 0.0
            ? result.summary.total_messages_processed / result.wall_clock_seconds
            : 0.0;
        const std::string digest = benchmark.lob_digest.empty()
            ? "<none>"
            : digestFingerprint(benchmark.lob_digest);

        out << result.strategy_name << ','
            << benchmark.input_format << ','
            << benchmark.processor << ','
            << result.summary.total_messages_processed << ','
            << result.summary.chronological_violations << ','
            << benchmark.unresolved_events << ','
            << std::fixed << std::setprecision(6) << result.wall_clock_seconds << ','
            << std::fixed << std::setprecision(2) << throughput << ','
            << digest << '\n';

        out.unsetf(std::ios::floatfield);
    }
}

} // namespace md
