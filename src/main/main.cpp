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

static DispatchResult run_dispatcher(EventQueue& q) {
    DispatchResult res;
    std::deque<MarketDataEvent> last_buf;

    while (true) {
        MarketDataEvent e = q.pop();
        if (e.ts_recv == MarketDataEvent::SENTINEL) break;

        if (res.count == 0) res.first_ts = e.ts_recv;
        res.last_ts = e.ts_recv;

        if (static_cast<int>(res.first_events.size()) < N_PREVIEW)
            res.first_events.push_back(e);

        last_buf.push_back(e);
        if (static_cast<int>(last_buf.size()) > N_PREVIEW)
            last_buf.pop_front();

        res.count++;
    }

    res.last_events = {last_buf.begin(), last_buf.end()};
    return res;
}

static void print_results(const char* label, const DispatchResult& res, double elapsed_sec) {
    std::printf("\n=== %s ===\n", label);
    std::printf("First %d events:\n", N_PREVIEW);
    for (auto& e : res.first_events) processMarketDataEvent(e);
    std::printf("Last %d events:\n", N_PREVIEW);
    for (auto& e : res.last_events) processMarketDataEvent(e);
    std::printf("Total messages : %zu\n", res.count);
    std::printf("First ts_recv  : %ld\n", res.first_ts);
    std::printf("Last ts_recv   : %ld\n", res.last_ts);
    std::printf("Wall time      : %.3f s\n", elapsed_sec);
    std::printf("Throughput     : %.0f msg/s\n", res.count / elapsed_sec);
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

static void run_standard(const std::filesystem::path& file) {
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
        if (count == 0) first_ts = e.ts_recv;
        last_ts = e.ts_recv;
        if (static_cast<int>(first_events.size()) < N_PREVIEW) first_events.push_back(e);
        last_deq.push_back(e);
        if (static_cast<int>(last_deq.size()) > N_PREVIEW) last_deq.pop_front();
        count++;
    }
    prod.join();

    std::printf("First %d events:\n", N_PREVIEW);
    for (auto& e : first_events) processMarketDataEvent(e);
    std::printf("Last %d events:\n", N_PREVIEW);
    for (auto& e : last_deq) processMarketDataEvent(e);
    std::printf("Total messages : %zu\n", count);
    std::printf("First ts_recv  : %ld\n", first_ts);
    std::printf("Last ts_recv   : %ld\n", last_ts);
}

static void run_benchmark(const std::vector<std::filesystem::path>& files) {
    auto make_producers = [&](std::vector<std::unique_ptr<EventQueue>>& queues,
                              std::vector<EventQueue*>&                  ptrs,
                              std::vector<std::unique_ptr<Producer>>&    producers) {
        queues.clear();
        ptrs.clear();
        producers.clear();
        for (auto& f : files) {
            auto& q = queues.emplace_back(std::make_unique<EventQueue>());
            ptrs.push_back(q.get());
            producers.emplace_back(std::make_unique<Producer>(f, *q));
        }
    };

    // Flat merger
    {
        std::vector<std::unique_ptr<EventQueue>> pqueues;
        std::vector<EventQueue*>                 pptrs;
        std::vector<std::unique_ptr<Producer>>   producers;
        make_producers(pqueues, pptrs, producers);

        FlatMerger merger(pptrs);

        for (auto& p : producers) p->start();

        DispatchResult res;
        auto disp_thread = std::thread([&] { res = run_dispatcher(merger.output()); });

        auto t0 = std::chrono::steady_clock::now();
        merger.run();
        disp_thread.join();
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

        for (auto& p : producers) p->start();
        merger.start();

        DispatchResult res;
        auto t0 = std::chrono::steady_clock::now();
        res     = run_dispatcher(merger.output());
        auto t1 = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(t1 - t0).count();

        merger.join();
        for (auto& p : producers) p->join();
        print_results("Hierarchy Merger", res, dt);
    }
}

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <file.mbo.json | folder>\n", argv[0]);
        return 1;
    }

    std::filesystem::path arg = argv[1];

    if (std::filesystem::is_regular_file(arg)) {
        run_standard(arg);
        return 0;
    }

    if (std::filesystem::is_directory(arg)) {
        auto files = collect_files(arg);
        if (files.empty()) {
            std::fprintf(stderr, "No .mbo.json files found in %s\n", arg.c_str());
            return 1;
        }
        std::printf("Found %zu files\n", files.size());
        run_benchmark(files);
        return 0;
    }

    std::fprintf(stderr, "Not a file or directory: %s\n", arg.c_str());
    return 1;
}
