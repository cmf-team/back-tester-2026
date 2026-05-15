#include "runners/StandardRunner.hpp"

#include "io/MmapFile.hpp"
#include "parsing/JsonParser.hpp"

#include <chrono>
#include <filesystem>
#include <ostream>
#include <stdexcept>

namespace md {

RunResult StandardRunner::run(
    const std::filesystem::path& file_path,
    IMarketDataEventProcessor& processor,
    bool verbose,
    std::ostream& err
) const {
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("input file does not exist: " + file_path.string());
    }
    if (!std::filesystem::is_regular_file(file_path)) {
        throw std::runtime_error("standard mode expects a file path: " + file_path.string());
    }

    MmapFile file(file_path);

    RunResult result;
    result.strategy_name = "standard";

    if (verbose) {
        err << "selected_mode=standard\n"
            << "input_file=" << file_path.string() << '\n'
            << "reader=mmap\n";
    }

    const auto started_at = std::chrono::steady_clock::now();

    std::size_t line_number = 0;
    while (auto line = file.nextLine()) {
        ++line_number;
        ++result.diagnostics.total_lines_read;

        const auto event = parseMarketDataEventLine(*line, line_number);
        processor.processMarketDataEvent(event);
        result.summary.observe(event);
    }

    const auto finished_at = std::chrono::steady_clock::now();
    result.wall_clock_seconds = std::chrono::duration<double>(finished_at - started_at).count();

    if (verbose) {
        err << "messages_processed=" << result.summary.total_messages_processed << '\n'
            << "chronological_violations=" << result.summary.chronological_violations << '\n';
    }

    return result;
}

} // namespace md
