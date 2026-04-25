// main — CLI entry point for the back-tester data-ingestion + LOB layer.
//
// Standard task (Task 1, single file):
//   back-tester <path/to/file.mbo.json>
//     Parses one NDJSON file, prints first/last 10 events and a summary.
//
// Hard task (Task 1+2, multi file):
//   back-tester <path/to/directory> [flags]
//     Spawns one producer thread per *.mbo.json file in the directory,
//     merges chronologically (Flat / Hierarchy / both), and optionally
//     dispatches every event to a per-instrument LimitOrderBook.
//
// Flags:
//   --strategy={flat|hierarchy|both}   merge strategy (default: both)
//   --verbose                          print every event (slow)
//   --lob                              build per-instrument LimitOrderBook
//   --snapshot-every=N                 emit BBO snapshot every N events
//                                      (implies --lob)
//   --sharded=N                        run a ShardedDispatcher with N workers
//                                      (1..8). Implies --lob. Replaces the
//                                      sequential per-instrument dispatcher.
//   --benchmark                        run sequential vs sharded x {2,4} for
//                                      each strategy and print a comparison
//                                      table. Per-instrument fingerprint must
//                                      match the sequential merger fingerprint.
//
// Examples:
//   back-tester data.mbo.json
//   back-tester data/extracted/ --lob --snapshot-every=100000
//   back-tester data/extracted/ --benchmark

#include "dispatcher/Dispatcher.hpp"
#include "dispatcher/ShardedDispatcher.hpp"
#include "market_data/Consumer.hpp"
#include "market_data/HardTask.hpp"
#include "market_data/MarketDataEvent.hpp"

#ifdef BACKTESTER_HAS_ARROW
#include "market_data/ArrowFeatherSource.hpp"
#include "market_data/EventSource.hpp"
#include "market_data/Merger.hpp"
#include <chrono>
#endif

#include <charconv>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

struct CliArgs
{
    std::filesystem::path path;
    std::string strategy = "both"; // flat | hierarchy | both
    bool verbose = false;
    bool lob = false;
    std::size_t snapshot_every = 0;
    std::size_t sharded = 0; // 0 = use sequential Dispatcher
    bool benchmark = false;
};

void printUsage(const char* prog)
{
    std::cerr << "Usage:\n"
              << "  " << prog << " <path-to-file.mbo.json>           # Standard\n"
              << "  " << prog
              << " <path-to-dir> [--strategy=<flat|hierarchy|both>]\n"
              << "                                  [--verbose] [--lob]\n"
              << "                                  [--snapshot-every=N]\n"
              << "                                  [--sharded=N] [--benchmark]\n";
}

bool startsWith(std::string_view s, std::string_view prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::size_t parseSizeT(std::string_view s, const char* flag)
{
    std::size_t v = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        throw std::runtime_error(std::string{flag} + " must be a non-negative int");
    return v;
}

CliArgs parseArgs(int argc, const char* argv[])
{
    CliArgs a;
    if (argc < 2)
        throw std::runtime_error("missing path argument");
    a.path = argv[1];
    for (int i = 2; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        if (startsWith(arg, "--strategy="))
        {
            a.strategy = std::string(arg.substr(std::strlen("--strategy=")));
        }
        else if (arg == "--verbose")
        {
            a.verbose = true;
        }
        else if (arg == "--lob")
        {
            a.lob = true;
        }
        else if (startsWith(arg, "--snapshot-every="))
        {
            a.snapshot_every = parseSizeT(arg.substr(std::strlen("--snapshot-every=")),
                                          "--snapshot-every");
            a.lob = true;
        }
        else if (startsWith(arg, "--sharded="))
        {
            a.sharded = parseSizeT(arg.substr(std::strlen("--sharded=")), "--sharded");
            a.lob = true;
        }
        else if (arg == "--benchmark")
        {
            a.benchmark = true;
            a.lob = true;
        }
        else
        {
            throw std::runtime_error("unknown flag: " + std::string{arg});
        }
    }
    if (a.strategy != "flat" && a.strategy != "hierarchy" && a.strategy != "both")
        throw std::runtime_error("--strategy must be flat|hierarchy|both");
    if (a.sharded > cmf::ShardedDispatcher::kMaxWorkers)
        throw std::runtime_error("--sharded must be in [0, 8]");
    return a;
}

int runStandard(const CliArgs& args)
{
    constexpr std::size_t kHead = 10;
    constexpr std::size_t kTail = 10;
    const auto s = cmf::runStandardTask(args.path, kHead, kTail, std::cout);
    std::cout << "\nDone. Processed " << s.total << " messages.\n";
    return EXIT_SUCCESS;
}

#ifdef BACKTESTER_HAS_ARROW
// Reads a single .feather file via ArrowFeatherSource and routes events
// through the per-instrument Dispatcher. Useful for the
// "ingestion speed: JSON vs Feather" comparison required by HW2.
int runFeatherFile(const CliArgs& args)
{
    const auto t0 = std::chrono::steady_clock::now();
    cmf::ArrowFeatherSource src(args.path);
    std::cout << "Feather file: " << args.path << " rows=" << src.total() << "\n";

    cmf::Dispatcher::Options opts;
    opts.snapshot_every = args.snapshot_every;
    opts.snapshot_out = args.snapshot_every > 0 ? &std::cout : nullptr;
    cmf::Dispatcher dispatcher(opts);

    cmf::MarketDataEvent ev;
    std::uint64_t total = 0;
    while (src.pop(ev))
    {
        dispatcher.dispatch(ev);
        ++total;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const auto secs =
        std::chrono::duration<double>(t1 - t0).count();
    std::cout << "feather: total=" << total << " elapsed=" << secs << " s "
              << "throughput=" << (secs > 0 ? total / secs : 0.0)
              << " msg/s\n";
    const auto stats = dispatcher.finalize();
    std::cout << "  events_routed=" << stats.events_routed
              << " orders_active=" << stats.orders_active
              << " instruments=" << stats.instruments_touched
              << " unresolved_iid=" << stats.unresolved_iid << "\n";
    cmf::printDispatcherSummary(std::cout, dispatcher);
    return EXIT_SUCCESS;
}
#endif

cmf::BenchmarkResult runOnce(const std::vector<std::filesystem::path>& files,
                             cmf::MergerKind kind, const CliArgs& args,
                             cmf::EventSink sink)
{
    return cmf::runHardTask(files, kind, args.verbose, 64 * 1024,
                            std::move(sink));
}

void printPipelineHeader(std::ostream& os, const std::string& mode,
                         cmf::MergerKind kind)
{
    os << "\n--- pipeline mode=" << mode
       << " merger=" << cmf::toString(kind) << " ---\n";
}

// Sequential per-instrument dispatcher on top of one merger.
cmf::BenchmarkResult runSequential(const std::vector<std::filesystem::path>& files,
                                   cmf::MergerKind kind, const CliArgs& args,
                                   bool print_summary)
{
    cmf::Dispatcher::Options opts;
    opts.snapshot_every = args.snapshot_every;
    opts.snapshot_out =
        args.snapshot_every > 0 ? &std::cout : nullptr;
    cmf::Dispatcher dispatcher(opts);

    auto sink = [&dispatcher](const cmf::MarketDataEvent& e)
    {
        dispatcher.dispatch(e);
    };

    printPipelineHeader(std::cout, "sequential", kind);
    auto r = runOnce(files, kind, args, sink);
    cmf::printBenchmarkResult(std::cout, r);
    const auto stats = dispatcher.finalize();
    std::cout << "  events_routed=" << stats.events_routed
              << " orders_active=" << stats.orders_active
              << " instruments=" << stats.instruments_touched
              << " unresolved_iid=" << stats.unresolved_iid << "\n";
    if (print_summary)
        cmf::printDispatcherSummary(std::cout, dispatcher);
    return r;
}

// Sharded dispatcher with N workers.
cmf::BenchmarkResult runSharded(const std::vector<std::filesystem::path>& files,
                                cmf::MergerKind kind, std::size_t num_workers,
                                const CliArgs& args)
{
    cmf::ShardedDispatcher dispatcher(num_workers);

    auto sink = [&dispatcher](const cmf::MarketDataEvent& e)
    {
        dispatcher.dispatch(e);
    };

    printPipelineHeader(std::cout, "sharded(" + std::to_string(num_workers) + ")",
                        kind);
    auto r = runOnce(files, kind, args, sink);
    cmf::printBenchmarkResult(std::cout, r);
    const auto stats = dispatcher.finalize();
    std::cout << "  events_routed=" << stats.events_routed
              << " unresolved_iid=" << stats.unresolved_iid << "\n";
    for (std::size_t i = 0; i < stats.per_worker_events.size(); ++i)
    {
        std::cout << "    worker[" << i << "] events=" << stats.per_worker_events[i]
                  << " instruments=" << stats.per_worker_instruments[i]
                  << " orders_active=" << stats.per_worker_orders_active[i] << "\n";
    }
    return r;
}

int runHard(const CliArgs& args)
{
    const auto files = cmf::listMboJsonFiles(args.path);
    if (files.empty())
    {
        std::cerr << "No *.mbo.json files found in " << args.path << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Discovered " << files.size() << " MBO NDJSON files in "
              << args.path << ":\n";
    for (const auto& f : files)
        std::cout << "  " << f.filename().string() << "\n";

    const bool do_flat = args.strategy == "flat" || args.strategy == "both";
    const bool do_hier = args.strategy == "hierarchy" || args.strategy == "both";

    // Benchmark mode runs the full grid: for each merger, do
    // baseline-no-lob, sequential-LOB, sharded(2)-LOB, sharded(4)-LOB.
    if (args.benchmark)
    {
        std::cout << "\n=== BENCHMARK MODE ===\n";
        cmf::BenchmarkResult last;
        auto kinds = std::vector<cmf::MergerKind>{};
        if (do_flat)
            kinds.push_back(cmf::MergerKind::Flat);
        if (do_hier)
            kinds.push_back(cmf::MergerKind::Hierarchy);
        for (auto kind : kinds)
        {
            printPipelineHeader(std::cout, "no-lob (baseline)", kind);
            auto r0 = runOnce(files, kind, args, {});
            cmf::printBenchmarkResult(std::cout, r0);
            auto r1 = runSequential(files, kind, args, /*print_summary=*/false);
            auto r2 = runSharded(files, kind, 2, args);
            auto r4 = runSharded(files, kind, 4, args);
            const bool fp_match =
                r0.fingerprint == r1.fingerprint &&
                r1.fingerprint == r2.fingerprint && r2.fingerprint == r4.fingerprint;
            std::cout << "  fingerprint_match=" << (fp_match ? "yes" : "NO") << "\n";
            if (!fp_match)
                return EXIT_FAILURE;
            last = r4;
        }
        (void)last;
        return EXIT_SUCCESS;
    }

    cmf::BenchmarkResult flat_r, hier_r;
    auto run_one = [&](cmf::MergerKind kind)
    {
        if (!args.lob)
        {
            printPipelineHeader(std::cout, "no-lob", kind);
            auto r = runOnce(files, kind, args, {});
            cmf::printBenchmarkResult(std::cout, r);
            return r;
        }
        if (args.sharded > 0)
            return runSharded(files, kind, args.sharded, args);
        return runSequential(files, kind, args, /*print_summary=*/true);
    };

    if (do_flat)
        flat_r = run_one(cmf::MergerKind::Flat);
    if (do_hier)
        hier_r = run_one(cmf::MergerKind::Hierarchy);

    if (do_flat && do_hier)
    {
        std::cout << "\n=== Cross-check ===\n"
                  << "Total match:       "
                  << (flat_r.total == hier_r.total ? "yes" : "NO") << "\n"
                  << "Fingerprint match: "
                  << (flat_r.fingerprint == hier_r.fingerprint ? "yes" : "NO")
                  << "\n";
        if (flat_r.total != hier_r.total ||
            flat_r.fingerprint != hier_r.fingerprint)
            return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, const char* argv[])
{
    try
    {
        const auto args = parseArgs(argc, argv);

        std::error_code ec;
        if (!std::filesystem::exists(args.path, ec))
        {
            std::cerr << "Path does not exist: " << args.path << "\n";
            return EXIT_FAILURE;
        }
        if (std::filesystem::is_directory(args.path, ec))
            return runHard(args);
#ifdef BACKTESTER_HAS_ARROW
        if (args.path.extension() == ".feather")
            return runFeatherFile(args);
#else
        if (args.path.extension() == ".feather")
        {
            std::cerr << "back-tester: built without Arrow support; rebuild with "
                         "-DENABLE_ARROW=ON to read .feather files.\n";
            return EXIT_FAILURE;
        }
#endif
        return runStandard(args);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "back-tester: fatal: " << ex.what() << "\n";
        printUsage(argc > 0 ? argv[0] : "back-tester");
        return EXIT_FAILURE;
    }
}
