#pragma once

#include "processing/AsyncSnapshotWriter.hpp"
#include "runners/InputFormat.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

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
    bool backtest_enabled{false};
    std::string strategy_name;
    std::uint64_t backtest_instrument_id{};
    std::uint64_t order_size{1};
    std::int64_t tick_size{1};
    std::int64_t quote_offset_ticks{1};
    std::uint64_t quote_interval_events{1};
    std::int64_t max_inventory{100};
    long double gamma{0.1L};
    long double sigma{1.0L};
    long double k{1.0L};
    long double horizon_seconds{1.0L};
    bool use_imbalance_skew{false};
    long double imbalance_alpha_ticks{};
};

} // namespace md
