#include "common/MarketDataEvent.hpp"
#include "ingestion/EventQueue.hpp"
#include "ingestion/FlatMerger.hpp"
#include "ingestion/HierarchyMerger.hpp"
#include "ingestion/JsonLineParser.hpp"
#include "ingestion/Producer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace cmf;

static constexpr int N_PREVIEW = 10;
static bool g_suppress_logs = false;

static void processMarketDataEvent(const MarketDataEvent& e) {
    std::printf("ts_recv=%ld ts_event=%ld order_id=%lu side=%c price=%.9f size=%u action=%c sym=%s\n",
                e.ts_recv, e.ts_event, e.order_id, e.side, e.price, e.size, e.action, e.symbol);
}

struct DispatchResult {
    std::size_t              count    = 0;
    NanoTime                 first_ts = 0;
    NanoTime                 last_ts  = 0;
    std::vector<MarketDataEvent> first_events;
    std::vector<MarketDataEvent> last_events;
};

template <typename Merger>
static DispatchResult run_dispatcher(Merger& merger) {
    DispatchResult res;
    res.first_events.reserve(N_PREVIEW);
    MarketDataEvent ring[N_PREVIEW];
    std::size_t ring_head = 0;
    std::size_t ring_len  = 0;
    MarketDataEvent e;

    while (merger.next(e)) {
        if (!g_suppress_logs) processMarketDataEvent(e);

        if (res.count == 0) res.first_ts = e.ts_recv;
        res.last_ts = e.ts_recv;

        if (static_cast<int>(res.first_events.size()) < N_PREVIEW)
            res.first_events.push_back(e);

        ring[(ring_head + ring_len) % N_PREVIEW] = e;
        if (ring_len < N_PREVIEW) ++ring_len;
        else ring_head = (ring_head + 1) % N_PREVIEW;

        res.count++;
    }

    res.last_events.reserve(ring_len);
    for (std::size_t i = 0; i < ring_len; ++i)
        res.last_events.push_back(ring[(ring_head + i) % N_PREVIEW]);
    return res;
}

static void print_results(const char* label, const DispatchResult& res, double elapsed_sec) {
    std::fprintf(stderr, "\n=== %s ===\n", label);
    if (!g_suppress_logs) {
        std::fprintf(stderr, "First %d events:\n", N_PREVIEW);
        for (auto& e : res.first_events) processMarketDataEvent(e);
        std::fprintf(stderr, "Last %d events:\n", N_PREVIEW);
        for (auto& e : res.last_events) processMarketDataEvent(e);
    }
    std::fprintf(stderr, "Total messages : %zu\n", res.count);
    std::fprintf(stderr, "First ts_recv  : %ld\n", res.first_ts);
    std::fprintf(stderr, "Last ts_recv   : %ld\n", res.last_ts);
    std::fprintf(stderr, "Wall time      : %.3f s\n", elapsed_sec);
    std::fprintf(stderr, "Throughput     : %.0f msg/s\n", res.count / elapsed_sec);
}

static std::vector<std::filesystem::path> collect_files(const std::filesystem::path& folder) {
    std::vector<std::filesystem::path> files;
    for (auto& entry : std::filesystem::directory_iterator(folder)) {
        auto p = entry.path();
        if (p.string().ends_with(".mbo.json"))
            files.push_back(p);
    }
    std::sort(files.begin(), files.end());
    return files;
}

// Each inner vector is one logical stream: its files are read sequentially by
// a single producer. Callers must ensure files within a stream are already
// sorted by ts_recv (typical for daily Databento files within one instrument folder).
using StreamList = std::vector<std::vector<std::filesystem::path>>;

static StreamList collect_streams(int argc, const char* argv[]) {
    StreamList streams;

    std::vector<std::filesystem::path> positional;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (a == "--suppress-logs") continue;
        positional.emplace_back(a);
    }

    if (positional.size() == 1 && std::filesystem::is_directory(positional[0])) {
        // Single folder: each file is its own stream. Safe default for folders
        // with overlapping-timestamp files across instruments.
        for (auto& f : collect_files(positional[0])) streams.push_back({f});
        return streams;
    }

    // Multiple args: each directory becomes one chained stream, each file is one stream.
    for (auto& p : positional) {
        if (std::filesystem::is_regular_file(p)) {
            streams.push_back({p});
        } else if (std::filesystem::is_directory(p)) {
            auto files = collect_files(p);
            if (!files.empty()) streams.push_back(std::move(files));
        }
    }
    return streams;
}

static void run_standard(const std::filesystem::path& file) {
    auto t0 = std::chrono::steady_clock::now();
    std::size_t count = 0;
    NanoTime    first_ts = 0, last_ts = 0;
    std::vector<MarketDataEvent> first_events;
    std::deque<MarketDataEvent>  last_deq;

    EventQueue q;
    Producer   prod(file, q);
    prod.start();

    while (true) {
        MarketDataEvent e = q.pop();
        if (e.ts_recv == MarketDataEvent::SENTINEL) break;
        if (!g_suppress_logs) processMarketDataEvent(e);
        if (count == 0) first_ts = e.ts_recv;
        last_ts = e.ts_recv;
        if (static_cast<int>(first_events.size()) < N_PREVIEW) first_events.push_back(e);
        last_deq.push_back(e);
        if (static_cast<int>(last_deq.size()) > N_PREVIEW) last_deq.pop_front();
        count++;
    }
    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(t1 - t0).count();

    std::fprintf(stderr, "First %d events:\n", N_PREVIEW);
    for (auto& e : first_events) processMarketDataEvent(e);
    std::fprintf(stderr, "Last %d events:\n", N_PREVIEW);
    for (auto& e : last_deq) processMarketDataEvent(e);

    std::fprintf(stderr, "\n--- summary ---\n");
    std::fprintf(stderr, "Total messages : %zu\n", count);
    std::fprintf(stderr, "First ts_recv  : %ld\n", first_ts);
    std::fprintf(stderr, "Last ts_recv   : %ld\n", last_ts);
    std::fprintf(stderr, "elapsed        : %.3f s  (%.0f msg/s)\n", dt, count / dt);
}

static void run_benchmark(const StreamList& streams) {
    auto make_producers = [&](std::vector<std::unique_ptr<EventQueue>>& queues,
                              std::vector<EventQueue*>&                  ptrs,
                              std::vector<std::unique_ptr<Producer>>&    producers) {
        queues.clear();
        ptrs.clear();
        producers.clear();
        for (auto& stream : streams) {
            auto& q = queues.emplace_back(std::make_unique<EventQueue>());
            ptrs.push_back(q.get());
            producers.emplace_back(std::make_unique<Producer>(stream, *q));
        }
    };

    // Flat merger
    {
        std::vector<std::unique_ptr<EventQueue>> pqueues;
        std::vector<EventQueue*>                 pptrs;
        std::vector<std::unique_ptr<Producer>>   producers;
        make_producers(pqueues, pptrs, producers);

        FlatMerger merger(pptrs);

        auto t0 = std::chrono::steady_clock::now();
        for (auto& p : producers) p->start();
        merger.start();

        DispatchResult res = run_dispatcher(merger);
        auto t1   = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(t1 - t0).count();

        for (auto& p : producers) p->join();
        print_results("Flat Merger", res, dt);
    }

    // Hierarchy merger
    {
        std::vector<std::unique_ptr<EventQueue>> pqueues;
        std::vector<EventQueue*>                 pptrs;
        std::vector<std::unique_ptr<Producer>>   producers;
        make_producers(pqueues, pptrs, producers);

        HierarchyMerger merger(pptrs);

        auto t0 = std::chrono::steady_clock::now();
        for (auto& p : producers) p->start();
        merger.start();

        DispatchResult res = run_dispatcher(merger);
        auto t1 = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(t1 - t0).count();

        merger.join();
        for (auto& p : producers) p->join();
        print_results("Hierarchy Merger", res, dt);
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr,
            "Usage:\n"
            "  %s <file.mbo.json> [--suppress-logs]          (single file)\n"
            "  %s <folder1> [folder2 ...] [--suppress-logs]  (each folder = one chained stream)\n",
            argv[0], argv[0]);
        return 1;
    }

    for (int i = 1; i < argc; ++i)
        if (std::string_view(argv[i]) == "--suppress-logs") g_suppress_logs = true;

    // Single-file mode: exactly one non-flag arg that is a regular file.
    int positional_count = 0;
    std::filesystem::path single_file;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (a == "--suppress-logs") continue;
        ++positional_count;
        single_file = a;
    }
    if (positional_count == 1 && std::filesystem::is_regular_file(single_file)) {
        run_standard(single_file);
        return 0;
    }

    StreamList streams = collect_streams(argc, argv);
    if (streams.empty()) {
        std::fprintf(stderr, "No valid files or folders provided\n");
        return 1;
    }

    std::size_t total_files = 0;
    for (auto& s : streams) total_files += s.size();
    std::fprintf(stderr, "Streams: %zu (total files: %zu)\n", streams.size(), total_files);
    run_benchmark(streams);
    return 0;
}
