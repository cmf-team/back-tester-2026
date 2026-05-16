#pragma once

#include "processing/IMarketDataEventProcessor.hpp"
#include "runners/InputFormat.hpp"
#include "runners/RunResult.hpp"

#include <filesystem>
#include <iosfwd>

namespace md {

class HierarchicalMergeRunner {
public:
    RunResult run(
        const std::filesystem::path& folder_path,
        IMarketDataEventProcessor& processor,
        bool verbose,
        std::ostream& err,
        InputFormat input_format = InputFormat::Json
    ) const;
};

} // namespace md
