#include "common/MarketDataEvent.hpp"
#include "data_layer/EventFlatMerger.hpp"
#include "data_layer/EventHierarchyMerger.hpp"
#include "data_layer/Producer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

using namespace cmf;

static constexpr int N_PREVIEW = 10;
static bool g_suppress_logs = true;

static void print_event(const MarketDataEvent& e) {
    std::printf(
        "ts_recv=%ld ts_event=%ld order_id=%lu side=%c price=%.9f size=%u action=%c sym=%s\n",
        e.ts_recv, e.ts_event, e.order_id, e.side, e.price, e.size, e.action, e.symbol
    );
}

struct Result {
    std::size_t count = 0;
    NanoTime first_ts = 0;
    NanoTime last_ts = 0;
};

template <typename Merger>
static Result run(Merger& merger,
                  std::vector<std::unique_ptr<Producer>>& producers)
{
    for (auto& p : producers) p->start();
    merger.start();

    MarketDataEvent e;
    Result r;

    auto t0 = std::chrono::steady_clock::now();

    while (merger.next(e)) {
        if (e.ts_recv == MarketDataEvent::SENTINEL)
            break;

        if (!g_suppress_logs)
            print_event(e);

        if (r.count == 0)
            r.first_ts = e.ts_recv;

        r.last_ts = e.ts_recv;
        ++r.count;
    }

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();

    for (auto& p : producers) p->join();
    merger.join();

    std::printf("\n=== RESULT ===\n");
    std::printf("Total messages : %zu\n", r.count);
    std::printf("Wall time (s)  : %.6f\n", sec);
    std::printf("Throughput     : %.0f msg/s\n", r.count / sec);

    return r;
}

static std::vector<std::filesystem::path>
collect_files(const std::filesystem::path& folder)
{
    std::vector<std::filesystem::path> files;

    for (auto& entry : std::filesystem::directory_iterator(folder)) {
        auto p = entry.path();
        if (p.string().ends_with(".mbo.json"))
            files.push_back(p);
    }

    std::sort(files.begin(), files.end());
    return files;
}

using StreamList = std::vector<std::vector<std::filesystem::path>>;

static StreamList build_streams(const std::vector<std::filesystem::path>& files) {
    StreamList streams;
    for (auto& f : files)
        streams.push_back({f});
    return streams;
}

struct Pipeline {
    std::vector<std::unique_ptr<SpscQueue<MarketDataEvent>>> queues;
    std::vector<SpscQueue<MarketDataEvent>*> ptrs;
    std::vector<std::unique_ptr<Producer>> producers;

    void build(const StreamList& streams) {
        queues.clear();
        ptrs.clear();
        producers.clear();

        for (auto& s : streams) {
            auto q = std::make_unique<SpscQueue<MarketDataEvent>>();
            ptrs.push_back(q.get());

            queues.push_back(std::move(q));
            producers.emplace_back(std::make_unique<Producer>(s, *queues.back()));
        }
    }
};

int main() {
    const std::filesystem::path DATA_DIR = "data";

    auto files = collect_files(DATA_DIR);

    if (files.empty()) {
        std::printf("No input files in %s\n", DATA_DIR.c_str());
        return 1;
    }

    std::printf("Files loaded: %zu\n", files.size());

    auto streams = build_streams(files);

    // Flat Merger
    {
        Pipeline p;
        p.build(streams);

        EventFlatMerger merger(p.ptrs);
        run(merger, p.producers);
    }

    // Hierarchy Merger
    {
        Pipeline p;
        p.build(streams);

        EventHierarchyMerger merger(p.ptrs);
        run(merger, p.producers);
    }

    return 0;
}