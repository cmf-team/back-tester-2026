#include "common/MarketDataEvent.hpp"
#include "data_layer/EventFlatMerger.hpp"
#include "data_layer/EventHierarchyMerger.hpp"
#include "data_layer/Producer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <vector>

using namespace cmf;

static constexpr const char* DATA_DIR = "test_data";

struct Stats {
    std::size_t messages = 0;
    double seconds = 0.0;
};

static std::vector<std::filesystem::path> collect_files(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;

    for (auto& e : std::filesystem::directory_iterator(dir)) {
        auto p = e.path();
        if (p.extension() == ".json" || p.string().ends_with(".mbo.json"))
            files.push_back(p);
    }

    std::sort(files.begin(), files.end());
    return files;
}

using StreamList = std::vector<std::vector<std::filesystem::path>>;

static StreamList build_streams(const std::vector<std::filesystem::path>& files) {
    StreamList streams;

    for (auto& f : files) {
        streams.push_back({f});
    }

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

static Stats run(auto& merger, std::vector<std::unique_ptr<Producer>>& producers) {
    for (auto& p : producers) p->start();
    merger.start();

    MarketDataEvent e;
    std::size_t count = 0;

    auto t0 = std::chrono::steady_clock::now();

    while (merger.next(e)) {
        if (e.ts_recv == MarketDataEvent::SENTINEL)
            break;
        ++count;
    }

    auto t1 = std::chrono::steady_clock::now();

    merger.join();
    for (auto& p : producers) p->join();

    double sec = std::chrono::duration<double>(t1 - t0).count();

    return {count, sec};
}

static void print(const char* name, const Stats& s) {
    std::printf("\n=== %s ===\n", name);
    std::printf("Total messages : %zu\n", s.messages);
    std::printf("Wall time (s)  : %.6f\n", s.seconds);
    std::printf("Throughput     : %.0f msg/s\n", s.messages / s.seconds);
}

int main() {
    auto files = collect_files(DATA_DIR);

    if (files.empty()) {
        std::printf("No input files in %s\n", DATA_DIR);
        return 1;
    }

    std::printf("Files loaded: %zu\n", files.size());

    auto streams = build_streams(files);

    // FlatMerger
    {
        Pipeline p;
        p.build(streams);

        EventFlatMerger merger(p.ptrs);

        auto stats = run(merger, p.producers);
        print("FlatMerger", stats);
    }

    // HierarchyMerger
    {
        Pipeline p;
        p.build(streams);

        EventHierarchyMerger merger(p.ptrs);

        auto stats = run(merger, p.producers);
        print("HierarchyMerger", stats);
    }

    return 0;
}