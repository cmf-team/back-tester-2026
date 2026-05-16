#include "app/ArgsParser.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace md {
namespace {

bool isHelp(const std::string& arg) {
    return arg == "--help" || arg == "-h" || arg == "help";
}

RunMode parseModeValue(const std::string& value) {
    if (value == "standard") {
        return RunMode::Standard;
    }
    if (value == "flat") {
        return RunMode::Flat;
    }
    if (value == "hierarchy" || value == "hierarchical") {
        return RunMode::Hierarchy;
    }
    if (value == "benchmark") {
        return RunMode::Benchmark;
    }

    throw ArgsError("unknown mode: " + value);
}

SnapshotWriterMode parseSnapshotWriterMode(const std::string& value) {
    if (value == "sync") {
        return SnapshotWriterMode::Sync;
    }
    if (value == "async") {
        return SnapshotWriterMode::Async;
    }

    throw ArgsError("unknown snapshot writer mode: " + value);
}

InputFormat parseInputFormat(const std::string& value) {
    if (value == "json") {
        return InputFormat::Json;
    }
    if (value == "feather") {
        return InputFormat::Feather;
    }

    throw ArgsError("unknown input format: " + value);
}

bool isSupportedBacktestStrategy(const std::string& value) {
    return value == "fixed_quote"
        || value == "avellaneda_stoikov"
        || value == "microprice_avellaneda_stoikov";
}

std::size_t parseSize(const std::string& value, const std::string& option) {
    try {
        std::size_t pos = 0;
        const auto parsed = std::stoull(value, &pos, 10);
        if (pos != value.size()) {
            throw ArgsError("invalid numeric value for " + option + ": " + value);
        }
        return static_cast<std::size_t>(parsed);
    } catch (const std::invalid_argument&) {
        throw ArgsError("invalid numeric value for " + option + ": " + value);
    } catch (const std::out_of_range&) {
        throw ArgsError("numeric value is out of range for " + option + ": " + value);
    }
}

std::int64_t parseInt64(const std::string& value, const std::string& option) {
    try {
        std::size_t pos = 0;
        const auto parsed = std::stoll(value, &pos, 10);
        if (pos != value.size()) {
            throw ArgsError("invalid numeric value for " + option + ": " + value);
        }
        return parsed;
    } catch (const std::invalid_argument&) {
        throw ArgsError("invalid numeric value for " + option + ": " + value);
    } catch (const std::out_of_range&) {
        throw ArgsError("numeric value is out of range for " + option + ": " + value);
    }
}

long double parseLongDouble(const std::string& value, const std::string& option) {
    try {
        std::size_t pos = 0;
        const auto parsed = std::stold(value, &pos);
        if (pos != value.size()) {
            throw ArgsError("invalid numeric value for " + option + ": " + value);
        }
        return parsed;
    } catch (const std::invalid_argument&) {
        throw ArgsError("invalid numeric value for " + option + ": " + value);
    } catch (const std::out_of_range&) {
        throw ArgsError("numeric value is out of range for " + option + ": " + value);
    }
}

void validateInput(const AppConfig& config) {
    if (config.mode == RunMode::Help) {
        return;
    }

    if (config.snapshot_interval_events == 0 && config.mode != RunMode::Benchmark) {
        throw ArgsError("snapshot interval must be greater than zero");
    }

    if (config.lob_workers == 0) {
        throw ArgsError("lob worker count must be greater than zero");
    }

    if (config.lob_workers != 1 && !config.use_lob_processor) {
        throw ArgsError("--lob-workers requires --lob");
    }

    if (config.backtest_enabled) {
        if (config.mode == RunMode::Benchmark) {
            throw ArgsError("--backtest is not supported with benchmark mode");
        }
        if (config.strategy_name.empty()) {
            throw ArgsError("--backtest requires --strategy");
        }
        if (!isSupportedBacktestStrategy(config.strategy_name)) {
            throw ArgsError("unknown backtest strategy: " + config.strategy_name);
        }
        if (config.backtest_instrument_id == 0) {
            throw ArgsError("--backtest requires --instrument-id");
        }
        if (config.order_size == 0) {
            throw ArgsError("--order-size must be greater than zero");
        }
        if (config.tick_size <= 0) {
            throw ArgsError("--tick-size must be greater than zero");
        }
        if (config.quote_offset_ticks < 0) {
            throw ArgsError("--quote-offset-ticks must be non-negative");
        }
        if (config.quote_interval_events == 0) {
            throw ArgsError("--quote-interval-events must be greater than zero");
        }
        if (config.max_inventory < 0) {
            throw ArgsError("--max-inventory must be non-negative");
        }
        if (config.gamma <= 0.0L) {
            throw ArgsError("--gamma must be greater than zero");
        }
        if (config.sigma < 0.0L) {
            throw ArgsError("--sigma must be non-negative");
        }
        if (config.k <= 0.0L) {
            throw ArgsError("--k must be greater than zero");
        }
        if (config.horizon_seconds < 0.0L) {
            throw ArgsError("--horizon-seconds must be non-negative");
        }
    } else if (!config.strategy_name.empty()) {
        throw ArgsError("--strategy requires --backtest");
    }

    if (config.input_path.empty()) {
        throw ArgsError("input path is required");
    }

    if (!std::filesystem::exists(config.input_path)) {
        throw ArgsError("input path does not exist: " + config.input_path.string());
    }

    if (config.mode == RunMode::Standard && !std::filesystem::is_regular_file(config.input_path)) {
        throw ArgsError("standard mode expects a file path: " + config.input_path.string());
    }

    if (config.mode == RunMode::Standard && config.input_format == InputFormat::Feather) {
        throw ArgsError("--input-format feather is supported only with flat, hierarchy, and benchmark modes");
    }

    if ((config.mode == RunMode::Flat || config.mode == RunMode::Hierarchy || config.mode == RunMode::Benchmark)
        && !std::filesystem::is_directory(config.input_path)) {
        throw ArgsError("hard-task modes expect a folder path: " + config.input_path.string());
    }
}

} // namespace

AppConfig ArgsParser::parse(int argc, char* argv[]) {
    if (argc <= 1) {
        throw ArgsError("missing arguments");
    }

    const std::string executable_name = argv[0] == nullptr ? "ingest" : argv[0];
    (void)executable_name;

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc - 1));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    if (args.size() == 1 && isHelp(args[0])) {
        AppConfig config;
        config.mode = RunMode::Help;
        return config;
    }

    // Backward-compatible Standard task mode: ./ingest /path/to/day.json
    if (args.size() == 1 && !args[0].starts_with('-')) {
        AppConfig config;
        config.mode = RunMode::Standard;
        config.input_path = args[0];
        validateInput(config);
        return config;
    }

    AppConfig config;
    bool mode_set = false;
    bool input_set = false;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (isHelp(arg)) {
            AppConfig help_config;
            help_config.mode = RunMode::Help;
            return help_config;
        }

        if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
            continue;
        }

        if (arg == "--lob") {
            config.use_lob_processor = true;
            continue;
        }

        if (arg == "--backtest") {
            config.backtest_enabled = true;
            continue;
        }

        if (arg == "--async-snapshots") {
            config.snapshot_writer_mode = SnapshotWriterMode::Async;
            continue;
        }

        if (arg == "--mode") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--mode requires a value");
            }
            config.mode = parseModeValue(args[++i]);
            mode_set = true;
            continue;
        }

        if (arg == "--input") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--input requires a path");
            }
            config.input_path = args[++i];
            input_set = true;
            continue;
        }

        if (arg == "--input-format") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--input-format requires a value");
            }
            config.input_format = parseInputFormat(args[++i]);
            continue;
        }

        if (arg == "--strategy") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--strategy requires a value");
            }
            config.strategy_name = args[++i];
            continue;
        }

        if (arg == "--instrument-id") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--instrument-id requires a number");
            }
            config.backtest_instrument_id = static_cast<std::uint64_t>(parseSize(args[++i], "--instrument-id"));
            continue;
        }

        if (arg == "--order-size") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--order-size requires a number");
            }
            config.order_size = static_cast<std::uint64_t>(parseSize(args[++i], "--order-size"));
            continue;
        }

        if (arg == "--tick-size") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--tick-size requires a number");
            }
            config.tick_size = parseInt64(args[++i], "--tick-size");
            continue;
        }

        if (arg == "--quote-offset-ticks") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--quote-offset-ticks requires a number");
            }
            config.quote_offset_ticks = parseInt64(args[++i], "--quote-offset-ticks");
            continue;
        }

        if (arg == "--quote-interval-events") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--quote-interval-events requires a number");
            }
            config.quote_interval_events = static_cast<std::uint64_t>(
                parseSize(args[++i], "--quote-interval-events")
            );
            continue;
        }

        if (arg == "--max-inventory") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--max-inventory requires a number");
            }
            config.max_inventory = parseInt64(args[++i], "--max-inventory");
            continue;
        }

        if (arg == "--gamma") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--gamma requires a number");
            }
            config.gamma = parseLongDouble(args[++i], "--gamma");
            continue;
        }

        if (arg == "--sigma") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--sigma requires a number");
            }
            config.sigma = parseLongDouble(args[++i], "--sigma");
            continue;
        }

        if (arg == "--k") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--k requires a number");
            }
            config.k = parseLongDouble(args[++i], "--k");
            continue;
        }

        if (arg == "--horizon-seconds") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--horizon-seconds requires a number");
            }
            config.horizon_seconds = parseLongDouble(args[++i], "--horizon-seconds");
            continue;
        }

        if (arg == "--imbalance-skew") {
            config.use_imbalance_skew = true;
            continue;
        }

        if (arg == "--imbalance-alpha-ticks") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--imbalance-alpha-ticks requires a number");
            }
            config.imbalance_alpha_ticks = parseLongDouble(args[++i], "--imbalance-alpha-ticks");
            continue;
        }

        if (arg == "--benchmark") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--benchmark requires a folder path");
            }
            config.mode = RunMode::Benchmark;
            config.input_path = args[++i];
            mode_set = true;
            input_set = true;
            continue;
        }

        if (arg == "--print-events") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--print-events requires a number");
            }
            config.max_events_to_print = parseSize(args[++i], "--print-events");
            continue;
        }

        if (arg == "--snapshot-depth") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--snapshot-depth requires a number");
            }
            config.snapshot_depth = parseSize(args[++i], "--snapshot-depth");
            continue;
        }

        if (arg == "--snapshot-interval-events") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--snapshot-interval-events requires a number");
            }
            config.snapshot_interval_events = parseSize(args[++i], "--snapshot-interval-events");
            continue;
        }

        if (arg == "--max-snapshots") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--max-snapshots requires a number");
            }
            config.max_snapshots = parseSize(args[++i], "--max-snapshots");
            continue;
        }

        if (arg == "--lob-workers") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--lob-workers requires a number");
            }
            config.lob_workers = parseSize(args[++i], "--lob-workers");
            continue;
        }

        if (arg == "--snapshot-writer") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--snapshot-writer requires a value");
            }
            config.snapshot_writer_mode = parseSnapshotWriterMode(args[++i]);
            continue;
        }

        if (arg == "--snapshot-output") {
            if (i + 1 >= args.size()) {
                throw ArgsError("--snapshot-output requires a path");
            }
            config.snapshot_output_path = args[++i];
            continue;
        }

        // Shorthand: ./ingest flat /path/to/folder
        if (!mode_set && (arg == "standard" || arg == "flat" || arg == "hierarchy" || arg == "hierarchical" || arg == "benchmark")) {
            config.mode = parseModeValue(arg);
            mode_set = true;
            if (i + 1 < args.size() && !args[i + 1].starts_with('-')) {
                config.input_path = args[++i];
                input_set = true;
            }
            continue;
        }

        if (!input_set && !arg.starts_with('-')) {
            config.input_path = arg;
            input_set = true;
            continue;
        }

        throw ArgsError("unknown argument: " + arg);
    }

    if (!mode_set) {
        throw ArgsError("mode is required unless using the single-file Standard shortcut");
    }

    if (config.mode == RunMode::Benchmark) {
        config.max_events_to_print = 0;
    }

    validateInput(config);
    return config;
}

std::string ArgsParser::usage(const std::string& executable_name) {
    std::ostringstream oss;
    oss << "Usage:\n"
        << "  " << executable_name << " <path_to_single_ndjson_file>\n"
        << "  " << executable_name << " --mode standard --input <path_to_single_ndjson_file> [--verbose] [--print-events N] [--lob] [--backtest --strategy fixed_quote|avellaneda_stoikov|microprice_avellaneda_stoikov --instrument-id ID --order-size N --tick-size PRICE_UNITS --quote-offset-ticks N --quote-interval-events N --max-inventory N --gamma X --sigma X --k X --horizon-seconds X --imbalance-skew --imbalance-alpha-ticks X] [--lob-workers N] [--snapshot-depth N] [--snapshot-interval-events N] [--max-snapshots N] [--snapshot-writer sync|async] [--async-snapshots] [--snapshot-output PATH]\n"
        << "  " << executable_name << " --mode flat --input <path_to_folder> [--input-format json|feather] [--verbose] [--print-events N] [--lob] [--backtest --strategy fixed_quote|avellaneda_stoikov|microprice_avellaneda_stoikov --instrument-id ID --order-size N --tick-size PRICE_UNITS --quote-offset-ticks N --quote-interval-events N --max-inventory N --gamma X --sigma X --k X --horizon-seconds X --imbalance-skew --imbalance-alpha-ticks X] [--lob-workers N] [--snapshot-depth N] [--snapshot-interval-events N] [--max-snapshots N] [--snapshot-writer sync|async] [--async-snapshots] [--snapshot-output PATH]\n"
        << "  " << executable_name << " --mode hierarchy --input <path_to_folder> [--input-format json|feather] [--verbose] [--print-events N] [--lob] [--backtest --strategy fixed_quote|avellaneda_stoikov|microprice_avellaneda_stoikov --instrument-id ID --order-size N --tick-size PRICE_UNITS --quote-offset-ticks N --quote-interval-events N --max-inventory N --gamma X --sigma X --k X --horizon-seconds X --imbalance-skew --imbalance-alpha-ticks X] [--lob-workers N] [--snapshot-depth N] [--snapshot-interval-events N] [--max-snapshots N] [--snapshot-writer sync|async] [--async-snapshots] [--snapshot-output PATH]\n"
        << "  " << executable_name << " --benchmark <path_to_folder> [--input-format json|feather] [--lob] [--lob-workers N]\n"
        << "\nShortcuts:\n"
        << "  " << executable_name << " standard <path_to_single_ndjson_file>\n"
        << "  " << executable_name << " flat <path_to_folder>\n"
        << "  " << executable_name << " hierarchy <path_to_folder>\n"
        << "  " << executable_name << " benchmark <path_to_folder>\n";
    return oss.str();
}

} // namespace md
