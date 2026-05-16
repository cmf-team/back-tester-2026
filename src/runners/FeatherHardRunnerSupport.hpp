#pragma once

#include "runners/HardRunnerSupport.hpp"

#include <filesystem>
#include <iosfwd>
#include <vector>

namespace md {

ProducerSet startFeatherProducerThreads(
    const std::vector<std::filesystem::path>& files,
    bool verbose,
    std::ostream& err
);

} // namespace md
