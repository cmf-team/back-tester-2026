#pragma once

#include <cstddef>
#include <filesystem>

namespace md {

enum class RunMode {
    Standard,
    Flat,
    Hierarchy,
    Benchmark,
    Help
};

struct AppConfig {
    RunMode mode{RunMode::Help};
    std::filesystem::path input_path;
    bool verbose{false};
    std::size_t max_events_to_print{10};
};

} // namespace md
