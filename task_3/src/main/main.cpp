#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Dispatcher.hpp"
#include "EventParser.hpp"
#include "FlatMerger.hpp"
#include "HierarchyMerger.hpp"
#include "LimitOrderBook.hpp"
#include "ShardedDispatcher.hpp"
#include "ThreadSafeQueue.hpp"

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

// ── helpers ────────────────────────────────────────────────────────────────

static std::vector<fs::path> json_files(const fs::path& dir)
{
    std::vector<fs::path> v;
    for (auto& e : fs::recursive_directory_iterator(dir))
        if (e.is_regular_file() && e.path().extension() == ".json")
            v.push_back(e.path());
    std::sort(v.begin(), v.end());
    return v;
}

static double elapsed(Clock::time_point t0)
{
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

// ── processMarketDataEvent (required by task spec) ─────────────────────────
// Called by the dispatcher for every event. Prints first/last N events.
static std::atomic<uint64_t> g_event_count{0};
static constexpr uint64_t PRINT_FIRST = 10, PRINT_LAST = 10;
static uint64_t g_total_events = 0; // set after parse
static std::mutex g_print_mu;

static void processMarketDataEvent(const MarketDataEvent& ev)
{
    uint64_t n = ++g_event_count;
    bool print_it = (n <= PRINT_FIRST) ||
                    (g_total_events > 0 && n > g_total_events - PRINT_LAST);
    if (!print_it)
        return;

    std::lock_guard lock(g_print_mu);
    std::printf("  [%6llu] ts=%-20llu  iid=%-8llu  oid=%-12llu  "
                "%-6s  %-4s  price=%-14.4f  qty=%lld\n",
                (unsigned long long)n,
                (unsigned long long)ev.ts,
                (unsigned long long)ev.instrument_id,
                (unsigned long long)ev.order_id,
                MarketDataEvent::type_str(ev.type),
                MarketDataEvent::side_str(ev.side),
                ev.price_decimal(),
                (long long)ev.qty);
}

// ── benchmark helper ───────────────────────────────────────────────────────

template <typename Merger>
static void run_benchmark(const char* name,
                          std::vector<std::vector<MarketDataEvent>> streams)
{
    auto t0 = Clock::now();
    Merger merger(std::move(streams));
    uint64_t count = 0;
    MarketDataEvent ev;
    while (merger.next(ev))
        ++count;
    double sec = elapsed(t0);
    std::printf("  %-20s  %10llu events  %6.3f s  %12.0f ev/s\n",
                name, (unsigned long long)count, sec,
                sec > 0 ? count / sec : 0.0);
}

// ── feather benchmark (calls Python script via system()) ──────────────────

static void run_feather_benchmark(const fs::path& data_dir)
{
    std::cout << "\n[4] Feather conversion & read-speed comparison\n";

    // Find convert script relative to binary or CWD
    std::string script = "scripts/convert_to_feather.py";
    if (!fs::exists(script))
        script = "../scripts/convert_to_feather.py";

    if (!fs::exists(script))
    {
        std::cout << "  convert_to_feather.py not found — skipping Feather benchmark\n";
        return;
    }

    std::string cmd = "py scripts/convert_to_feather.py --benchmark " + data_dir.string();
    std::cout << "  Running: " << cmd << "\n";
    auto t0 = Clock::now();
    int rc = std::system(cmd.c_str());
    if (rc != 0)
    {
        cmd = "python scripts/convert_to_feather.py --benchmark " + data_dir.string();
        t0 = Clock::now();
        rc = std::system(cmd.c_str());
    }
    if (rc != 0)
    {
        cmd = "python3 scripts/convert_to_feather.py --benchmark " + data_dir.string();
        t0 = Clock::now();
        rc = std::system(cmd.c_str());
    }
    if (rc != 0)
        std::cout << "  Python not found or script error (code " << rc << ")\n"
                  << "  Run manually: py scripts/convert_to_feather.py --benchmark "
                  << data_dir.string() << "\n";
    else
        std::printf("  Feather benchmark total wall time: %.2f s\n", elapsed(t0));
}

// ── main ───────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <data_directory>\n";
        return 1;
    }

    fs::path data_dir = argv[1];
    auto files = json_files(data_dir);
    if (files.empty())
    {
        std::cerr << "No JSON files in " << data_dir << "\n";
        return 1;
    }

    std::cout << "=== High-Performance Multi-Instrument LOB Backtester ===\n";
    std::cout << "Data directory : " << data_dir << "\n";
    std::cout << "Files found    : " << files.size() << "\n\n";

    // ── [1] Producer threads ─────────────────────────────────────────────
    std::cout << "[1] Reading " << files.size() << " file(s) in parallel...\n";
    auto t_read = Clock::now();

    std::vector<std::vector<MarketDataEvent>> streams(files.size());
    {
        std::vector<std::thread> producers;
        std::mutex mu;
        for (std::size_t i = 0; i < files.size(); ++i)
        {
            producers.emplace_back([&, i]
                                   {
                try {
                    streams[i] = parse_file(files[i].string());
                    std::lock_guard lock(mu);
                    std::cout << "  [producer " << i << "] "
                              << files[i].filename().string()
                              << " → " << streams[i].size() << " events\n";
                } catch (std::exception& e) {
                    std::lock_guard lock(mu);
                    std::cerr << "  [producer " << i << "] ERROR: " << e.what() << "\n";
                } });
        }
        for (auto& t : producers)
            t.join();
    }

    uint64_t total_parsed = 0;
    for (auto& s : streams)
        total_parsed += s.size();
    g_total_events = total_parsed;

    std::printf("  Total parsed: %llu events in %.3f s\n\n",
                (unsigned long long)total_parsed, elapsed(t_read));

    // ── [2] Merger benchmark ─────────────────────────────────────────────
    std::cout << "[2] Merger benchmark (merge-only, no LOB):\n";
    run_benchmark<FlatMerger>("FlatMerger", streams);
    run_benchmark<HierarchyMerger>("HierarchyMerger", streams);
    std::cout << "\n";

    // ── [3] Full pipeline: FlatMerger → Queue → Dispatcher ──────────────
    std::cout << "[3] Full pipeline  (FlatMerger → Queue → Dispatcher → LOB):\n";
    std::cout << "    First " << PRINT_FIRST
              << " and last " << PRINT_LAST << " events:\n";

    g_event_count = 0;
    ThreadSafeQueue<MarketDataEvent> queue(200'000);
    Dispatcher dispatcher(queue, /*snapshot_every=*/50'000);

    // Async snapshot callback
    std::atomic<int> snap_count{0};
    dispatcher.set_snapshot_callback(
        [&snap_count](uint64_t iid, const LimitOrderBook& lob, uint64_t n)
        {
            ++snap_count;
            if (snap_count <= 2)
            { // only print first 2 to avoid noise
                std::printf("\n  [SNAPSHOT] instrument=%llu  event#%llu\n",
                            (unsigned long long)iid, (unsigned long long)n);
                lob.print_snapshot(3);
            }
        });

    auto t_pipe = Clock::now();

    // Dispatcher thread
    std::thread disp_thread([&]
                            { dispatcher.run(); });

    // Merger feeds the queue (runs in main thread)
    {
        auto streams_copy = streams; // copy for pipeline run
        FlatMerger merger(std::move(streams_copy));
        MarketDataEvent ev;
        while (merger.next(ev))
        {
            processMarketDataEvent(ev); // print first/last per task spec
            queue.push(ev);
        }
        queue.set_done();
    }
    disp_thread.join();

    double pipe_sec = elapsed(t_pipe);

    // ── [3] Summary ──────────────────────────────────────────────────────
    std::cout << "\n[3] Pipeline summary:\n";
    std::printf("    Instruments (LOBs) : %zu\n", dispatcher.lob_count());
    std::printf("    Events processed   : %llu\n",
                (unsigned long long)dispatcher.total_events());
    std::printf("    Wall time          : %.3f s\n", pipe_sec);
    std::printf("    Throughput         : %.0f ev/s\n",
                pipe_sec > 0 ? dispatcher.total_events() / pipe_sec : 0.0);
    std::printf("    Async snapshots    : %d\n\n", snap_count.load());

    dispatcher.print_summary();

    // ── [4] Bonus: Sharded dispatcher benchmark ──────────────────────────
    std::cout << "\n[4] BONUS: Sharded dispatcher benchmark\n";
    std::cout << "    (instruments sharded across 2 and 4 worker threads)\n\n";

    for (std::size_t num_workers : {2u, 4u})
    {
        g_event_count = 0;
        ThreadSafeQueue<MarketDataEvent> sharded_queue(200'000);
        ShardedDispatcher sharded(sharded_queue, num_workers);

        auto t_shard = Clock::now();

        std::thread shard_thread([&]
                                 { sharded.run(); });

        {
            auto streams_copy = streams;
            FlatMerger merger(std::move(streams_copy));
            MarketDataEvent ev;
            while (merger.next(ev))
                sharded_queue.push(ev);
            sharded_queue.set_done();
        }
        shard_thread.join();

        double shard_sec = elapsed(t_shard);
        std::printf("  %zu workers:  %llu events  %.3f s  %.0f ev/s\n",
                    num_workers,
                    (unsigned long long)sharded.total_events(),
                    shard_sec,
                    shard_sec > 0 ? sharded.total_events() / shard_sec : 0.0);
        sharded.print_worker_stats();
    }

    // Compare with sequential
    std::printf("\n  Sequential:  %llu events  %.3f s  %.0f ev/s\n",
                (unsigned long long)dispatcher.total_events(),
                pipe_sec,
                pipe_sec > 0 ? dispatcher.total_events() / pipe_sec : 0.0);
    std::cout << "\n  → Speedup sharded vs sequential shown above\n";

    // ── [5] Feather benchmark (Python via system()) ──────────────────────
    run_feather_benchmark(data_dir);

    return 0;
}
