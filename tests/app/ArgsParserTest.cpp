#include "TestSupport.hpp"

namespace md::test {

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

    const auto explicit_hierarchy = parseArgs({"ingest", "--mode", "hierarchy", "--input", dir.string()});
    require(explicit_hierarchy.mode == RunMode::Hierarchy, "explicit hierarchy parsed");

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

    std::filesystem::remove_all(dir);
}

} // namespace md::test
