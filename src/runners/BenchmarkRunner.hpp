#pragma once

#include "runners/InputFormat.hpp"
#include "runners/RunResult.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <vector>

namespace md {

std::vector<BenchmarkResult> runLoggingBenchmark(
    const std::filesystem::path& folder_path,
    InputFormat input_format,
    bool verbose,
    std::ostream& err
);

std::vector<BenchmarkResult> runLobBenchmark(
    const std::filesystem::path& folder_path,
    InputFormat input_format,
    bool verbose,
    std::ostream& err,
    std::size_t lob_workers = 1
);

} // namespace md
