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

void testArgsParserAcceptsBacktestFixedQuote() {
    const auto dir = makeTempDir("args_backtest_fixed_quote");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    const auto config = parseArgs({
        "ingest",
        "--mode", "standard",
        "--input", file.string(),
        "--backtest",
        "--strategy", "fixed_quote",
        "--instrument-id", "1",
        "--order-size", "10",
        "--tick-size", "1000000000",
        "--quote-offset-ticks", "1",
    });

    require(config.mode == RunMode::Standard, "args_parser_accepts_backtest_fixed_quote: mode");
    require(config.backtest_enabled, "args_parser_accepts_backtest_fixed_quote: backtest flag");
    require(config.strategy_name == "fixed_quote", "args_parser_accepts_backtest_fixed_quote: strategy");
    require(config.backtest_instrument_id == 1, "args_parser_accepts_backtest_fixed_quote: instrument id");
    require(config.order_size == 10, "args_parser_accepts_backtest_fixed_quote: order size");
    require(config.tick_size == 1'000'000'000, "args_parser_accepts_backtest_fixed_quote: tick size");
    require(config.quote_offset_ticks == 1, "args_parser_accepts_backtest_fixed_quote: quote offset");

    std::filesystem::remove_all(dir);
}

void testArgsParserAcceptsBacktestAvellanedaStoikov() {
    const auto dir = makeTempDir("args_backtest_as");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    const auto config = parseArgs({
        "ingest",
        "--mode", "standard",
        "--input", file.string(),
        "--backtest",
        "--strategy", "avellaneda_stoikov",
        "--instrument-id", "442",
        "--order-size", "2",
        "--tick-size", "10000",
        "--quote-interval-events", "5",
        "--gamma", "0.2",
        "--sigma", "1.5",
        "--k", "0.8",
        "--horizon-seconds", "10",
    });

    require(config.strategy_name == "avellaneda_stoikov", "args_parser_accepts_backtest_as: strategy");
    require(config.quote_interval_events == 5, "args_parser_accepts_backtest_as: interval");
    require(config.gamma == 0.2L, "args_parser_accepts_backtest_as: gamma");
    require(config.sigma == 1.5L, "args_parser_accepts_backtest_as: sigma");
    require(config.k == 0.8L, "args_parser_accepts_backtest_as: k");
    require(config.horizon_seconds == 10.0L, "args_parser_accepts_backtest_as: horizon");

    std::filesystem::remove_all(dir);
}

void testArgsParserAcceptsBacktestMicropriceAvellanedaStoikov() {
    const auto dir = makeTempDir("args_backtest_microprice_as");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    const auto config = parseArgs({
        "ingest",
        "--mode", "standard",
        "--input", file.string(),
        "--backtest",
        "--strategy", "microprice_avellaneda_stoikov",
        "--instrument-id", "442",
        "--tick-size", "10000",
        "--imbalance-skew",
        "--imbalance-alpha-ticks", "0.5",
    });

    require(
        config.strategy_name == "microprice_avellaneda_stoikov",
        "args_parser_accepts_backtest_microprice_as: strategy"
    );
    require(config.use_imbalance_skew, "args_parser_accepts_backtest_microprice_as: imbalance flag");
    require(config.imbalance_alpha_ticks == 0.5L, "args_parser_accepts_backtest_microprice_as: alpha");

    std::filesystem::remove_all(dir);
}

void testArgsParserRejectsBacktestWithoutStrategy() {
    const auto dir = makeTempDir("args_backtest_no_strategy");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    expectArgsErrorContains(
        {
            "ingest",
            "--mode", "standard",
            "--input", file.string(),
            "--backtest",
            "--instrument-id", "1",
        },
        "--backtest requires --strategy",
        "args_parser_rejects_backtest_without_strategy"
    );

    std::filesystem::remove_all(dir);
}

void testArgsParserRejectsBacktestWithoutInstrumentId() {
    const auto dir = makeTempDir("args_backtest_no_instrument");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    expectArgsErrorContains(
        {
            "ingest",
            "--mode", "standard",
            "--input", file.string(),
            "--backtest",
            "--strategy", "fixed_quote",
        },
        "--backtest requires --instrument-id",
        "args_parser_rejects_backtest_without_instrument_id"
    );

    std::filesystem::remove_all(dir);
}

void testArgsParserAcceptsTickSize() {
    const auto dir = makeTempDir("args_backtest_tick_size");
    const auto file = dir / "sample.ndjson";
    writeFile(file, line(1, 1) + "\n");

    const auto config = parseArgs({
        "ingest",
        "--mode", "standard",
        "--input", file.string(),
        "--backtest",
        "--strategy", "fixed_quote",
        "--instrument-id", "442",
        "--tick-size", "10000",
    });

    require(config.backtest_enabled, "args_parser_accepts_tick_size: backtest flag");
    require(config.tick_size == 10'000, "args_parser_accepts_tick_size: tick size");

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

void testUsageMentionsBacktestOptions() {
    const auto usage = ArgsParser::usage("ingest");

    requireContains(usage, "--backtest", "usage mentions backtest");
    requireContains(usage, "--strategy fixed_quote", "usage mentions fixed quote strategy");
    requireContains(usage, "--instrument-id ID", "usage mentions instrument id");
    requireContains(usage, "--order-size N", "usage mentions order size");
    requireContains(usage, "--tick-size PRICE_UNITS", "usage mentions tick size");
    requireContains(usage, "--quote-offset-ticks N", "usage mentions quote offset ticks");
    requireContains(usage, "--quote-interval-events N", "usage mentions quote interval");
    requireContains(usage, "--max-inventory N", "usage mentions max inventory");
    requireContains(usage, "avellaneda_stoikov", "usage mentions avellaneda strategy");
    requireContains(usage, "microprice_avellaneda_stoikov", "usage mentions microprice strategy");
    requireContains(usage, "--gamma X", "usage mentions gamma");
    requireContains(usage, "--sigma X", "usage mentions sigma");
    requireContains(usage, "--k X", "usage mentions k");
    requireContains(usage, "--horizon-seconds X", "usage mentions horizon");
    requireContains(usage, "--imbalance-skew", "usage mentions imbalance skew");
    requireContains(usage, "--imbalance-alpha-ticks X", "usage mentions imbalance alpha");
}

} // namespace md::test
