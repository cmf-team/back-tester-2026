#include "TestSupport.hpp"

#include "runners/ResultPrinter.hpp"

#include <sstream>

namespace md::test {

namespace {

RunResult standardResultWithTiming() {
    RunResult result;
    result.strategy_name = "standard";
    result.summary.total_messages_processed = 1000;
    result.summary.chronological_violations = 0;
    result.wall_clock_seconds = 0.5;
    return result;
}

} // namespace

void testResultPrinterPrintsTimingForStandardMode() {
    std::ostringstream out;

    printRunResult(standardResultWithTiming(), out, false, 0);

    const auto text = out.str();
    requireContains(text, "Summary", "standard timing summary marker");
    requireContains(text, "total_messages_processed=1000", "standard timing total events");
    requireContains(text, "wall_clock_seconds=0.500000", "standard timing wall clock");
}

void testResultPrinterPrintsThroughputForStandardMode() {
    std::ostringstream out;

    printRunResult(standardResultWithTiming(), out, false, 0);

    requireContains(
        out.str(),
        "throughput_messages_per_second=2000.000000",
        "standard timing throughput"
    );
}

} // namespace md::test
