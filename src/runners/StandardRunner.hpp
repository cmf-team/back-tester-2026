#pragma once

#include "processing/IMarketDataEventProcessor.hpp"
#include "runners/RunResult.hpp"

#include <filesystem>
#include <iosfwd>

namespace md {

class StandardRunner {
public:
    RunResult run(
        const std::filesystem::path& file_path,
        IMarketDataEventProcessor& processor,
        bool verbose,
        std::ostream& err
    ) const;
};

} // namespace md
