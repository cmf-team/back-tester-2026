#include "TestSupport.hpp"

namespace md::test {
namespace {

void expectArgsErrorContains(
    const std::vector<std::string>& args,
    std::string_view expected_message,
    const std::string& case_name
) {
    try {
        parseArgs(args);
    } catch (const ArgsError& e) {
        requireContains(e.what(), expected_message, case_name + ": unexpected error message");
        return;
    }

    throw std::runtime_error("expected ArgsError: " + case_name);
}

} // namespace

void testArgsParserAllSupportedForms() {
    const auto dir = makeTempDir("args");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    expectArgsError({"ingest"}, "no args");
    expectArgsError({"ingest", "--mode", "standard"}, "missing input");
    expectArgsError({"ingest", "--mode", "unknown", "--input", file.string()}, "unknown mode");

    const auto shortcut_standard = parseArgs({"ingest", file.string()});
    require(shortcut_standard.mode == RunMode::Standard, "single path maps to standard");
    require(shortcut_standard.input_path == file, "single path input preserved");

    const auto shorthand_standard = parseArgs({"ingest", "standard", file.string()});
    require(shorthand_standard.mode == RunMode::Standard, "standard shortcut parsed");

    const auto shorthand_flat = parseArgs({"ingest", "flat", dir.string()});
    require(shorthand_flat.mode == RunMode::Flat, "flat shortcut parsed");

    const auto shorthand_hierarchy = parseArgs({"ingest", "hierarchy", dir.string(), "--print-events", "3"});
    require(shorthand_hierarchy.mode == RunMode::Hierarchy, "hierarchy shortcut parsed");
    require(shorthand_hierarchy.max_events_to_print == 3, "print-events parsed");

    const auto shorthand_benchmark = parseArgs({"ingest", "benchmark", dir.string()});
    require(shorthand_benchmark.mode == RunMode::Benchmark, "benchmark shortcut parsed");
    require(shorthand_benchmark.max_events_to_print == 0, "benchmark shortcut suppresses event printing");

    const auto explicit_standard = parseArgs({"ingest", "--mode", "standard", "--input", file.string(), "--verbose", "--print-events", "0"});
    require(explicit_standard.mode == RunMode::Standard, "explicit standard parsed");
    require(explicit_standard.verbose, "verbose flag parsed");
    require(explicit_standard.max_events_to_print == 0, "zero print-events parsed");

    const auto explicit_flat = parseArgs({"ingest", "--mode", "flat", "--input", dir.string()});
    require(explicit_flat.mode == RunMode::Flat, "explicit flat parsed");
    require(explicit_flat.input_format == InputFormat::Json, "default input format is json");

    const auto explicit_hierarchy = parseArgs({
        "ingest",
        "--mode", "hierarchy",
        "--input", dir.string(),
        "--input-format", "feather",
    });
    require(explicit_hierarchy.mode == RunMode::Hierarchy, "explicit hierarchy parsed");
    require(explicit_hierarchy.input_format == InputFormat::Feather, "feather input format parsed");

    const auto explicit_benchmark = parseArgs({"ingest", "--benchmark", dir.string()});
    require(explicit_benchmark.mode == RunMode::Benchmark, "explicit benchmark parsed");
    require(explicit_benchmark.max_events_to_print == 0, "benchmark suppresses sample logging");

    const auto help = parseArgs({"ingest", "--help"});
    require(help.mode == RunMode::Help, "help parsed");
    requireContains(ArgsParser::usage("ingest"), "--mode standard", "usage includes explicit standard");

    expectArgsError({"ingest", "--mode", "standard", "--input", dir.string()}, "standard with folder path");
    expectArgsError({"ingest", "standard", dir.string()}, "standard shortcut with folder path");
    expectArgsError({"ingest", "--mode", "flat", "--input", file.string()}, "flat with file path");
    expectArgsError({"ingest", "--mode", "hierarchy", "--input", file.string()}, "hierarchy with file path");
    expectArgsError({"ingest", "--benchmark", file.string()}, "benchmark with file path");
    expectArgsError({"ingest", "--print-events", "NaN", "--mode", "standard", "--input", file.string()}, "invalid print-events");
    expectArgsError({"ingest", "--mode", "flat", "--input", dir.string(), "--input-format", "csv"}, "unknown input format");
    expectArgsError({"ingest", "--mode", "standard", "--input", file.string(), "--input-format", "feather"}, "standard feather input format");

    std::filesystem::remove_all(dir);
}

void testArgsParserAcceptsLobFlag() {
    const auto dir = makeTempDir("args_lob");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    const auto config = parseArgs({"ingest", "--mode", "standard", "--input", file.string(), "--lob"});

    require(config.mode == RunMode::Standard, "lob mode keeps standard mode");
    require(config.use_lob_processor, "lob flag parsed");
    require(config.snapshot_depth == 5, "default snapshot depth");
    require(config.snapshot_interval_events == 100'000, "default snapshot interval");
    require(config.max_snapshots == 5, "default max snapshots");
    require(config.lob_workers == 1, "default lob worker count");

    std::filesystem::remove_all(dir);
}

void testArgsParserAcceptsSnapshotOptions() {
    const auto dir = makeTempDir("args_lob_options");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    const auto config = parseArgs({
        "ingest",
        "--mode", "standard",
        "--input", file.string(),
        "--lob",
        "--snapshot-depth", "3",
        "--snapshot-interval-events", "2",
        "--max-snapshots", "1",
        "--lob-workers", "4",
    });

    require(config.use_lob_processor, "lob flag parsed with snapshot options");
    require(config.snapshot_depth == 3, "snapshot depth parsed");
    require(config.snapshot_interval_events == 2, "snapshot interval parsed");
    require(config.max_snapshots == 1, "max snapshots parsed");
    require(config.lob_workers == 4, "lob worker count parsed");

    const auto async_config = parseArgs({
        "ingest",
        "--mode", "standard",
        "--input", file.string(),
        "--lob",
        "--snapshot-writer", "async",
        "--snapshot-output", (dir / "snapshots.txt").string(),
    });
    require(async_config.snapshot_writer_mode == SnapshotWriterMode::Async, "async snapshot writer parsed");
    require(
        async_config.snapshot_output_path == (dir / "snapshots.txt"),
        "snapshot output path parsed"
    );

    const auto shortcut_config = parseArgs({
        "ingest",
        "--mode", "standard",
        "--input", file.string(),
        "--lob",
        "--async-snapshots",
    });
    require(shortcut_config.snapshot_writer_mode == SnapshotWriterMode::Async, "async snapshot shortcut parsed");

    expectArgsErrorContains(
        {"ingest", "--mode", "standard", "--input", file.string(), "--lob", "--snapshot-writer", "later"},
        "unknown snapshot writer mode",
        "unknown snapshot writer mode"
    );
    expectArgsErrorContains(
        {"ingest", "--mode", "standard", "--input", file.string(), "--lob", "--lob-workers", "0"},
        "lob worker count must be greater than zero",
        "zero lob worker count"
    );
    expectArgsErrorContains(
        {"ingest", "--mode", "standard", "--input", file.string(), "--lob-workers", "2"},
        "--lob-workers requires --lob",
        "lob workers without lob"
    );

    std::filesystem::remove_all(dir);
}

void testArgsParserRejectsSnapshotIntervalZero() {
    const auto dir = makeTempDir("args_lob_zero_interval");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    expectArgsErrorContains(
        {"ingest", "--mode", "standard", "--input", file.string(), "--lob", "--snapshot-interval-events", "0"},
        "snapshot interval must be greater than zero",
        "zero snapshot interval"
    );

    std::filesystem::remove_all(dir);
}

void testArgsParserAcceptsLobWithHardModes() {
    const auto dir = makeTempDir("args_lob_hard_modes");

    const auto flat = parseArgs({"ingest", "--mode", "flat", "--input", dir.string(), "--lob"});
    require(flat.mode == RunMode::Flat, "lob flat mode parsed");
    require(flat.use_lob_processor, "lob flat flag parsed");

    const auto hierarchy = parseArgs({"ingest", "--mode", "hierarchy", "--input", dir.string(), "--lob"});
    require(hierarchy.mode == RunMode::Hierarchy, "lob hierarchy mode parsed");
    require(hierarchy.use_lob_processor, "lob hierarchy flag parsed");

    std::filesystem::remove_all(dir);
}

void testArgsParserAcceptsLobWithBenchmarkMode() {
    const auto dir = makeTempDir("args_lob_benchmark");

    const auto config = parseArgs({
        "ingest",
        "--benchmark", dir.string(),
        "--lob",
        "--lob-workers", "2",
        "--snapshot-depth", "99",
        "--snapshot-interval-events", "0",
        "--max-snapshots", "99",
    });

    require(config.mode == RunMode::Benchmark, "lob benchmark mode parsed");
    require(config.use_lob_processor, "lob benchmark flag parsed");
    require(config.lob_workers == 2, "lob benchmark worker count parsed");
    require(config.max_events_to_print == 0, "lob benchmark suppresses event printing");
    require(config.snapshot_interval_events == 0, "lob benchmark accepts ignored zero snapshot interval");

    std::filesystem::remove_all(dir);
}

void testUsageMentionsLobOptions() {
    const auto usage = ArgsParser::usage("ingest");

    requireContains(usage, "--lob", "usage mentions lob flag");
    requireContains(usage, "--snapshot-depth N", "usage mentions snapshot depth");
    requireContains(usage, "--snapshot-interval-events N", "usage mentions snapshot interval");
    requireContains(usage, "--max-snapshots N", "usage mentions max snapshots");
    requireContains(usage, "--lob-workers N", "usage mentions lob workers");
    requireContains(usage, "--async-snapshots", "usage mentions async snapshots");
    requireContains(usage, "--snapshot-writer sync|async", "usage mentions snapshot writer");
    requireContains(usage, "--snapshot-output PATH", "usage mentions snapshot output");
    requireContains(usage, "--input-format json|feather", "usage mentions input format");
}

} // namespace md::test
