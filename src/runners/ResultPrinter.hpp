#pragma once

#include "runners/RunResult.hpp"

#include <iosfwd>
#include <vector>

namespace md {

void printRunResult(const RunResult& result, std::ostream& out, bool verbose, std::size_t max_events_to_print);
void printBenchmarkResults(const std::vector<RunResult>& results, std::ostream& out);

} // namespace md
