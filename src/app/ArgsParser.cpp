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

void validateInput(const AppConfig& config) {
    if (config.mode == RunMode::Help) {
        return;
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
        return AppConfig{RunMode::Help, {}, false, 10};
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
            return AppConfig{RunMode::Help, {}, false, 10};
        }

        if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
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
        << "  " << executable_name << " --mode standard --input <path_to_single_ndjson_file> [--verbose] [--print-events N]\n"
        << "  " << executable_name << " --mode flat --input <path_to_folder> [--verbose] [--print-events N]\n"
        << "  " << executable_name << " --mode hierarchy --input <path_to_folder> [--verbose] [--print-events N]\n"
        << "  " << executable_name << " --benchmark <path_to_folder>\n"
        << "\nShortcuts:\n"
        << "  " << executable_name << " standard <path_to_single_ndjson_file>\n"
        << "  " << executable_name << " flat <path_to_folder>\n"
        << "  " << executable_name << " hierarchy <path_to_folder>\n"
        << "  " << executable_name << " benchmark <path_to_folder>\n";
    return oss.str();
}

} // namespace md
