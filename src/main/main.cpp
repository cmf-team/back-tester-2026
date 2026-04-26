#include "common/MarketDataEvent.hpp"
#include "dispatch/Dispatcher.hpp"
#include "dispatch/EventSink.hpp"
#include "dispatch/LobRegistry.hpp"
#include "dispatch/ShardedDispatcher.hpp"
#include "dispatch/SnapshotSink.hpp"
#include "ingestion/EventQueue.hpp"
#include "ingestion/FlatMerger.hpp"
#include "ingestion/HierarchyMerger.hpp"
#include "ingestion/JsonLineParser.hpp"
#include "ingestion/Producer.hpp"
#include "lob/LimitOrderBook.hpp"
#include "lob/OrderIndex.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
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

static bool is_flag(std::string_view a) {
    return a == "--suppress-logs" || a == "--lob" || a == "--bench"
        || a == "--snapshot-out" || a == "--snapshot-every"
        || a == "--shards";
}

static bool consumes_value(std::string_view a) {
    return a == "--snapshot-out" || a == "--snapshot-every" || a == "--shards";
}

static StreamList collect_streams(int argc, const char* argv[]) {
    StreamList streams;

    std::vector<std::filesystem::path> positional;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (is_flag(a)) {
            if (consumes_value(a)) ++i;
            continue;
        }
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

static void print_bbo(const LobRegistry& reg, std::size_t max_books, const char* tag, NanoTime ts) {
    std::fprintf(stderr, "--- %s @ ts=%ld ---\n", tag, ts);
    std::size_t k = 0;
    reg.for_each([&](uint32_t id, const LimitOrderBook& b) {
        if (k++ >= max_books) return;
        double bp = 0, ap = 0;
        LimitOrderBook::AggQty bq = 0, aq = 0;
        const bool hb = b.best_bid(bp, bq);
        const bool ha = b.best_ask(ap, aq);
        if (hb && ha)
            std::fprintf(stderr, "  inst=%u  bid %.6f x %lu  |  ask %.6f x %lu  (lvl b=%zu a=%zu)\n",
                         id, bp, bq, ap, aq, b.bid_levels(), b.ask_levels());
        else if (hb)
            std::fprintf(stderr, "  inst=%u  bid %.6f x %lu  |  ask -\n", id, bp, bq);
        else if (ha)
            std::fprintf(stderr, "  inst=%u  bid -  |  ask %.6f x %lu\n", id, ap, aq);
        else
            std::fprintf(stderr, "  inst=%u  empty\n", id);
    });
}

class PeriodicBboSink : public EventSink {
public:
    PeriodicBboSink(const LobRegistry& reg, std::size_t every) noexcept
        : reg_(reg), every_(every) {}
    void on_event(const MarketDataEvent& e, const LimitOrderBook&) override {
        ++n_;
        if (every_ && n_ % every_ == 0)
            print_bbo(reg_, 5, "snapshot", e.ts_recv);
    }
private:
    const LobRegistry& reg_;
    std::size_t        every_;
    std::size_t        n_ = 0;
};

template <class Merger>
static void drive_lob(Merger& merger, LobRegistry& reg, OrderIndex& idx, EventSink& sink,
                      const char* label) {
    auto t0 = std::chrono::steady_clock::now();
    Dispatcher<Merger> disp(merger, reg, idx, sink);
    disp.run();
    auto t1 = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(t1 - t0).count();

    std::fprintf(stderr, "\n=== %s ===\n", label);
    std::fprintf(stderr, "Events processed : %zu\n", disp.events_processed());
    std::fprintf(stderr, "Trades seen      : %zu\n", disp.trades_seen());
    std::fprintf(stderr, "Orphans skipped  : %zu\n", disp.orphans_skipped());
    std::fprintf(stderr, "First ts_recv    : %ld\n", disp.first_ts());
    std::fprintf(stderr, "Last  ts_recv    : %ld\n", disp.last_ts());
    std::fprintf(stderr, "Books            : %zu\n", reg.size());
    std::fprintf(stderr, "Index live orders: %zu\n", idx.size());
    std::fprintf(stderr, "Wall time        : %.3f s\n", dt);
    std::fprintf(stderr, "Throughput       : %.0f msg/s\n", disp.events_processed() / dt);

    print_bbo(reg, reg.size(), "final BBO", disp.last_ts());
}

struct LobOpts {
    std::size_t           bbo_print_every  = 0;
    std::size_t           snapshot_every   = 0;
    std::filesystem::path snapshot_out;
    std::size_t           shards           = 0;
};

static void run_lob_pipeline(const StreamList& streams, const LobOpts& opts) {
    const std::size_t snapshot_every = opts.bbo_print_every;
    std::vector<std::unique_ptr<EventQueue>> queues;
    std::vector<EventQueue*>                 ptrs;
    std::vector<std::unique_ptr<Producer>>   producers;
    queues.reserve(streams.size());
    ptrs.reserve(streams.size());
    producers.reserve(streams.size());
    for (auto& stream : streams) {
        auto& q = queues.emplace_back(std::make_unique<EventQueue>());
        ptrs.push_back(q.get());
        producers.emplace_back(std::make_unique<Producer>(stream, *q));
    }

    FlatMerger      merger(ptrs);
    LobRegistry     reg;
    OrderIndex      idx;
    PeriodicBboSink periodic(reg, snapshot_every);
    SnapshotSink    snap(reg, opts.snapshot_out, opts.snapshot_every);
    NullSink        null_sink;
    MultiSink       multi;
    if (snapshot_every)       multi.add(&periodic);
    if (opts.snapshot_every)  multi.add(&snap);
    EventSink& sink = (snapshot_every || opts.snapshot_every)
                          ? static_cast<EventSink&>(multi)
                          : static_cast<EventSink&>(null_sink);

    snap.start();
    for (auto& p : producers) p->start();
    merger.start();

    if (opts.shards <= 1) {
        drive_lob(merger, reg, idx, sink, "Flat Merger -> LOB");
    } else {
        auto t0 = std::chrono::steady_clock::now();
        ShardedDispatcher<FlatMerger> sd(merger, idx, opts.shards);
        sd.run();
        auto t1 = std::chrono::steady_clock::now();
        const double dt = std::chrono::duration<double>(t1 - t0).count();
        std::fprintf(stderr, "\n=== Sharded Dispatcher (N=%zu) ===\n", opts.shards);
        std::fprintf(stderr, "Events processed : %zu\n", sd.events_processed());
        std::fprintf(stderr, "Trades seen      : %zu\n", sd.trades_seen());
        std::fprintf(stderr, "Orphans skipped  : %zu\n", sd.orphans_skipped());
        std::fprintf(stderr, "First ts_recv    : %ld\n", sd.first_ts());
        std::fprintf(stderr, "Last  ts_recv    : %ld\n", sd.last_ts());
        std::fprintf(stderr, "Books total      : %zu\n", sd.total_books());
        std::fprintf(stderr, "Index live orders: %zu\n", idx.size());
        std::fprintf(stderr, "Wall time        : %.3f s\n", dt);
        std::fprintf(stderr, "Throughput       : %.0f msg/s\n", sd.events_processed() / dt);
        for (std::size_t i = 0; i < sd.shards(); ++i)
            std::fprintf(stderr, "Shard %zu books: %zu\n", i, sd.registry(i).size());
    }

    for (auto& p : producers) p->join();
    snap.stop();
    if (opts.snapshot_every) {
        std::fprintf(stderr, "Snapshots written: %zu -> %s\n",
                     snap.frames_published(), opts.snapshot_out.c_str());
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr,
            "Usage:\n"
            "  %s <file.mbo.json> [--suppress-logs]                   (single file, ingest only)\n"
            "  %s <folder1> [folder2 ...] [--suppress-logs] [--lob]   (each folder = one chained stream)\n"
            "    --bench   ingest-only benchmark (default for multi-folder)\n"
            "    --lob     run full Producer->Merger->Dispatcher->LOB pipeline\n",
            argv[0], argv[0]);
        return 1;
    }

    bool    lob_mode   = false;
    bool    bench_mode = false;
    LobOpts opts;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if      (a == "--suppress-logs")  g_suppress_logs = true;
        else if (a == "--lob")            lob_mode = true;
        else if (a == "--bench")          bench_mode = true;
        else if (a == "--snapshot-out"   && i + 1 < argc) { opts.snapshot_out  = argv[++i]; }
        else if (a == "--snapshot-every" && i + 1 < argc) { opts.snapshot_every = std::stoul(argv[++i]); }
        else if (a == "--shards"         && i + 1 < argc) { opts.shards = std::stoul(argv[++i]); }
    }

    int positional_count = 0;
    std::filesystem::path single_file;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (is_flag(a)) continue;
        ++positional_count;
        single_file = a;
    }
    if (positional_count == 1 && std::filesystem::is_regular_file(single_file) && !lob_mode) {
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

    if (lob_mode) {
        opts.bbo_print_every = g_suppress_logs ? 0 : 1'000'000;
        run_lob_pipeline(streams, opts);
    } else {
        (void)bench_mode;
        run_benchmark(streams);
    }
    return 0;
}
