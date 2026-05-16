#pragma once

#include "processing/AsyncSnapshotWriter.hpp"
#include "runners/InputFormat.hpp"

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
    InputFormat input_format{InputFormat::Json};
    bool verbose{false};
    std::size_t max_events_to_print{10};
    bool use_lob_processor{false};
    std::size_t snapshot_depth{5};
    std::size_t snapshot_interval_events{100'000};
    std::size_t max_snapshots{5};
    SnapshotWriterMode snapshot_writer_mode{SnapshotWriterMode::Sync};
    std::filesystem::path snapshot_output_path;
    std::size_t lob_workers{1};
};

} // namespace md
