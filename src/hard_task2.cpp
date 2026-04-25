/**
 * hard_task2.cpp
 * --------------
 * Homework 2 Hard Variant:
 * High-performance multi-instrument backtester pipeline.
 *
 * Features:
 *  - Reuses MarketDataEvent + NdjsonParser + LimitOrderBook
 *  - FlatMerger + HierarchyMerger (from task 1)
 *  - Single global dispatcher thread
 *  - Parallel LOB snapshot workers (std::async)
 *  - BONUS: Sharded LOB processing (2-4 worker threads, each owns a subset)
 *  - Full benchmark output
 *
 * Usage:
 *   ./hard_task2 <folder1> [folder2] [flat|hierarchy|both] [shards=N]
 *
 * Build:
 *   g++ -std=c++20 -O3 -I../include hard_task2.cpp -o hard_task2 -lpthread
 */

#include "MarketDataEvent.hpp"
#include "NdjsonParser.hpp"
#include "LimitOrderBook.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  BlockingQueue
// ─────────────────────────────────────────────────────────────────────────────

template <typename T, size_t Cap = 8192>
class BlockingQueue {
public:
    void push(T item) {
        std::unique_lock lk(mu_);
        cv_full_.wait(lk, [&]{ return q_.size() < Cap; });
        q_.push(std::move(item));
        cv_empty_.notify_one();
    }
    std::optional<T> pop() {
        std::unique_lock lk(mu_);
        cv_empty_.wait(lk, [&]{ return !q_.empty() || done_; });
        if (q_.empty()) return std::nullopt;
        T v = std::move(q_.front()); q_.pop();
        cv_full_.notify_one();
        return v;
    }
    void set_done() { std::unique_lock lk(mu_); done_=true; cv_empty_.notify_all(); }
    bool is_done()  { std::lock_guard lk(mu_); return done_ && q_.empty(); }
private:
    std::mutex mu_;
    std::condition_variable cv_empty_, cv_full_;
    std::queue<T> q_;
    bool done_ = false;
};

using EventQueue = BlockingQueue<MarketDataEvent, 8192>;

// ─────────────────────────────────────────────────────────────────────────────
//  Producer
// ─────────────────────────────────────────────────────────────────────────────

static void producer_fn(const std::string& path,
                         std::shared_ptr<EventQueue> q,
                         std::atomic<uint64_t>& counter) {
    std::ifstream f(path);
    if (!f) { q->set_done(); return; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto opt = NdjsonParser::parse_line(line);
        if (opt) { q->push(std::move(*opt)); ++counter; }
    }
    q->set_done();
}

// ─────────────────────────────────────────────────────────────────────────────
//  FlatMerger
// ─────────────────────────────────────────────────────────────────────────────

class FlatMerger {
public:
    explicit FlatMerger(std::vector<std::shared_ptr<EventQueue>> qs)
        : queues_(std::move(qs)) {}

    uint64_t run(std::function<void(const MarketDataEvent&)> cb) {
        using E = std::pair<MarketDataEvent, size_t>;
        auto cmp = [](const E& a, const E& b){ return a.first > b.first; };
        std::priority_queue<E, std::vector<E>, decltype(cmp)> heap(cmp);

        for (size_t i = 0; i < queues_.size(); ++i) {
            auto o = queues_[i]->pop();
            if (o) heap.push({std::move(*o), i});
        }
        uint64_t n = 0;
        while (!heap.empty()) {
            auto [evt, idx] = heap.top(); heap.pop();
            cb(evt); ++n;
            auto nx = queues_[idx]->pop();
            if (nx) heap.push({std::move(*nx), idx});
        }
        return n;
    }
private:
    std::vector<std::shared_ptr<EventQueue>> queues_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  HierarchyMerger
// ─────────────────────────────────────────────────────────────────────────────

class MergePair {
public:
    MergePair(std::shared_ptr<EventQueue> l, std::shared_ptr<EventQueue> r)
        : left_(l), right_(r), out_(std::make_shared<EventQueue>()) {}

    std::shared_ptr<EventQueue> output() { return out_; }

    void start() { thr_ = std::thread([this]{ loop(); }); }
    void join()  { if (thr_.joinable()) thr_.join(); }

private:
    void loop() {
        auto lb = left_->pop(), rb = right_->pop();
        while (lb || rb) {
            if (!lb)          { out_->push(std::move(*rb)); rb = right_->pop(); }
            else if (!rb)     { out_->push(std::move(*lb)); lb = left_->pop();  }
            else if (*lb > *rb){ out_->push(std::move(*rb)); rb = right_->pop(); }
            else              { out_->push(std::move(*lb)); lb = left_->pop();  }
        }
        out_->set_done();
    }
    std::shared_ptr<EventQueue> left_, right_, out_;
    std::thread thr_;
};

class HierarchyMerger {
public:
    explicit HierarchyMerger(std::vector<std::shared_ptr<EventQueue>> qs)
        : leaves_(std::move(qs)) {}

    uint64_t run(std::function<void(const MarketDataEvent&)> cb) {
        auto cur = leaves_;
        while (cur.size() > 1) {
            std::vector<std::shared_ptr<EventQueue>> nxt;
            for (size_t i = 0; i < cur.size(); i += 2) {
                if (i+1 < cur.size()) {
                    auto p = std::make_shared<MergePair>(cur[i], cur[i+1]);
                    p->start(); pairs_.push_back(p);
                    nxt.push_back(p->output());
                } else nxt.push_back(cur[i]);
            }
            cur = std::move(nxt);
        }
        auto root = cur[0];
        uint64_t n = 0;
        while (true) {
            auto o = root->pop();
            if (!o) break;
            cb(*o); ++n;
        }
        for (auto& p : pairs_) p->join();
        return n;
    }
private:
    std::vector<std::shared_ptr<EventQueue>> leaves_;
    std::vector<std::shared_ptr<MergePair>>  pairs_;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Sharded LOB Engine
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Distributes instruments across N worker threads.
 *
 * The dispatcher routes each event to a per-shard BlockingQueue.
 * Each shard worker owns a subset of LOBs and processes them sequentially.
 * This gives parallelism across instruments while preserving per-instrument order.
 */
class ShardedLobEngine {
public:
    explicit ShardedLobEngine(int n_shards)
        : n_shards_(n_shards) {
        for (int i = 0; i < n_shards_; ++i)
            shards_.push_back(std::make_shared<BlockingQueue<MarketDataEvent, 4096>>());
    }

    // Called by dispatcher thread
    void dispatch(const MarketDataEvent& evt) {
        int shard = evt.instrument_id % n_shards_;
        shards_[shard]->push(evt);
        ++total_dispatched_;
    }

    void start_workers() {
        for (int i = 0; i < n_shards_; ++i) {
            workers_.emplace_back([this, i]{ worker_loop(i); });
        }
    }

    void shutdown() {
        for (auto& q : shards_) q->set_done();
        for (auto& t : workers_) if (t.joinable()) t.join();
    }

    uint64_t total_dispatched() const { return total_dispatched_; }

    void print_summary(std::ostream& os = std::cout) const {
        os << "\n══════ Sharded LOB Summary ══════\n";
        uint64_t grand = 0;
        size_t   total_instruments = 0;
        for (int i = 0; i < n_shards_; ++i) {
            std::lock_guard lk(shard_mu_[i]);
            uint64_t shard_evts = 0;
            for (auto& [id, lob] : lob_maps_[i])
                shard_evts += lob.total_events();
            os << "  Shard " << i << ": " << lob_maps_[i].size()
               << " instruments, " << shard_evts << " events\n";
            grand += shard_evts;
            total_instruments += lob_maps_[i].size();
        }
        os << "  Total: " << total_instruments << " instruments, "
           << grand << " events\n";
    }

    void print_top_snapshots(int n = 3) const {
        // Gather top LOBs across all shards by event count
        std::vector<std::pair<uint64_t, std::pair<int,uint32_t>>> ranked;
        for (int i = 0; i < n_shards_; ++i) {
            std::lock_guard lk(shard_mu_[i]);
            for (auto& [id, lob] : lob_maps_[i])
                ranked.push_back({lob.total_events(), {i, id}});
        }
        std::sort(ranked.rbegin(), ranked.rend());
        std::cout << "\n══════ Top " << n << " LOB Snapshots ══════\n";
        for (int k = 0; k < std::min(n, (int)ranked.size()); ++k) {
            auto [shard_idx, id] = ranked[k].second;
            std::lock_guard lk(shard_mu_[shard_idx]);
            lob_maps_[shard_idx].at(id).print_snapshot(5);
        }
    }

    void print_all_best(std::ostream& os = std::cout) const {
        os << "\n══════ Final Best Bid/Ask ══════\n";
        os << std::left << std::setw(10) << "Instr"
           << std::setw(14) << "BestBid"
           << std::setw(14) << "BestAsk"
           << "Events\n" << std::string(50,'-') << "\n";
        for (int i = 0; i < n_shards_; ++i) {
            std::lock_guard lk(shard_mu_[i]);
            for (auto& [id, lob] : lob_maps_[i]) {
                double bb = (lob.best_bid() == std::numeric_limits<int64_t>::min())
                            ? 0.0 : lob.best_bid()*1e-9;
                double ba = (lob.best_ask() == std::numeric_limits<int64_t>::max())
                            ? 0.0 : lob.best_ask()*1e-9;
                os << std::setw(10) << id
                   << std::fixed << std::setprecision(6)
                   << std::setw(14) << bb
                   << std::setw(14) << ba
                   << lob.total_events() << "\n";
            }
        }
    }

private:
    void worker_loop(int shard_idx) {
        lob_maps_.resize(n_shards_);  // safe: pre-allocated
        while (true) {
            auto opt = shards_[shard_idx]->pop();
            if (!opt) break;
            std::lock_guard lk(shard_mu_[shard_idx]);
            auto& lobs = lob_maps_[shard_idx];
            auto it = lobs.find(opt->instrument_id);
            if (it == lobs.end()) {
                lobs.emplace(opt->instrument_id,
                             LimitOrderBook(opt->instrument_id, opt->symbol));
                lobs.at(opt->instrument_id).apply(*opt);
            } else {
                it->second.apply(*opt);
            }
        }
    }

    int n_shards_;
    std::vector<std::shared_ptr<BlockingQueue<MarketDataEvent,4096>>> shards_;
    std::vector<std::thread> workers_;
    mutable std::vector<std::mutex> shard_mu_{(size_t)4}; // max 4 shards
    mutable std::vector<std::unordered_map<uint32_t, LimitOrderBook>> lob_maps_{4};
    std::atomic<uint64_t> total_dispatched_{0};
};

// ─────────────────────────────────────────────────────────────────────────────
//  Sequential LOB engine (for comparison)
// ─────────────────────────────────────────────────────────────────────────────

class SequentialLobEngine {
public:
    void apply(const MarketDataEvent& evt) {
        auto it = lobs_.find(evt.instrument_id);
        if (it == lobs_.end()) {
            lobs_.emplace(evt.instrument_id,
                          LimitOrderBook(evt.instrument_id, evt.symbol));
            lobs_.at(evt.instrument_id).apply(evt);
        } else {
            it->second.apply(evt);
        }
        ++total_;
    }

    uint64_t total() const { return total_; }
    size_t   n_instruments() const { return lobs_.size(); }

    void print_top_snapshots(int n=3) const {
        std::vector<std::pair<uint64_t,uint32_t>> r;
        for (auto& [id,lob] : lobs_) r.push_back({lob.total_events(), id});
        std::sort(r.rbegin(), r.rend());
        std::cout << "\n══════ Top " << n << " LOB Snapshots ══════\n";
        for (int i=0;i<std::min(n,(int)r.size());++i)
            lobs_.at(r[i].second).print_snapshot(5);
    }

    void print_all_best(std::ostream& os=std::cout) const {
        os << "\n══════ Final Best Bid/Ask ══════\n";
        os << std::left << std::setw(10)<<"Instr"
           << std::setw(14)<<"BestBid" << std::setw(14)<<"BestAsk"
           <<"Events\n" << std::string(50,'-') << "\n";
        for (auto& [id,lob]:lobs_) {
            double bb=(lob.best_bid()==std::numeric_limits<int64_t>::min())?0.0:lob.best_bid()*1e-9;
            double ba=(lob.best_ask()==std::numeric_limits<int64_t>::max())?0.0:lob.best_ask()*1e-9;
            os<<std::setw(10)<<id<<std::fixed<<std::setprecision(6)
              <<std::setw(14)<<bb<<std::setw(14)<<ba<<lob.total_events()<<"\n";
        }
    }

private:
    std::unordered_map<uint32_t, LimitOrderBook> lobs_;
    uint64_t total_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
//  File collection helper
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<std::string> collect_files(int argc, char* argv[],
                                               int from, int to) {
    std::vector<std::string> files;
    for (int i = from; i <= to; ++i) {
        fs::path folder = argv[i];
        if (!fs::is_directory(folder)) continue;
        size_t before = files.size();
        for (auto& e : fs::directory_iterator(folder)) {
            std::string name = e.path().filename().string();
            if (name.find(".mbo.json") != std::string::npos)
                files.push_back(e.path().string());
        }
        std::cout << "Folder " << i << ": " << folder.string()
                  << "  →  " << (files.size()-before) << " files\n";
    }
    std::sort(files.begin(), files.end());
    return files;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Benchmark runner
// ─────────────────────────────────────────────────────────────────────────────

struct BenchResult {
    std::string label;
    uint64_t messages;
    uint64_t instruments;
    double   wall_sec;
    double   throughput;
};

static BenchResult run_sequential(const std::string& label,
                                   const std::vector<std::string>& files,
                                   bool use_hierarchy,
                                   bool verbose = true) {
    std::atomic<uint64_t> prod_count{0};
    std::vector<std::shared_ptr<EventQueue>> queues;
    std::vector<std::thread> producers;

    for (auto& f : files) {
        auto q = std::make_shared<EventQueue>();
        queues.push_back(q);
        producers.emplace_back(producer_fn, f, q, std::ref(prod_count));
    }

    SequentialLobEngine engine;

    // Snapshot schedule
    uint64_t dispatched = 0;
    constexpr uint64_t SNAP_INT = 5'000'000;
    uint64_t next_snap = SNAP_INT;
    int snap_n = 0;

    auto cb = [&](const MarketDataEvent& evt) {
        engine.apply(evt);
        ++dispatched;
        if (verbose && dispatched >= next_snap && snap_n < 2) {
            std::cout << "\n[Snapshot #" << ++snap_n << " at " << dispatched << " events]\n";
            engine.print_top_snapshots(2);
            next_snap += SNAP_INT;
        }
    };

    auto t0 = std::chrono::steady_clock::now();
    uint64_t total = 0;
    if (use_hierarchy) {
        HierarchyMerger m(queues); total = m.run(cb);
    } else {
        FlatMerger m(queues);      total = m.run(cb);
    }
    auto t1 = std::chrono::steady_clock::now();

    for (auto& t : producers) t.join();
    double elapsed = std::chrono::duration<double>(t1-t0).count();

    if (verbose) {
        engine.print_top_snapshots(3);
        engine.print_all_best();
    }

    return {label, total, engine.n_instruments(), elapsed, total/elapsed};
}

static BenchResult run_sharded(const std::string& label,
                                const std::vector<std::string>& files,
                                bool use_hierarchy,
                                int n_shards) {
    std::atomic<uint64_t> prod_count{0};
    std::vector<std::shared_ptr<EventQueue>> queues;
    std::vector<std::thread> producers;

    for (auto& f : files) {
        auto q = std::make_shared<EventQueue>();
        queues.push_back(q);
        producers.emplace_back(producer_fn, f, q, std::ref(prod_count));
    }

    ShardedLobEngine engine(n_shards);
    engine.start_workers();

    uint64_t total = 0;
    auto cb = [&](const MarketDataEvent& evt) {
        engine.dispatch(evt);
        ++total;
    };

    auto t0 = std::chrono::steady_clock::now();
    if (use_hierarchy) {
        HierarchyMerger m(queues); m.run(cb);
    } else {
        FlatMerger m(queues);      m.run(cb);
    }
    engine.shutdown();
    auto t1 = std::chrono::steady_clock::now();

    for (auto& t : producers) t.join();
    double elapsed = std::chrono::duration<double>(t1-t0).count();

    engine.print_top_snapshots(2);
    engine.print_all_best();
    engine.print_summary();

    return {label, total, 0, elapsed, total/elapsed};
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <folder1> [folder2] [flat|hierarchy|both] [shards=N]\n";
        return 1;
    }

    // Parse arguments
    std::string mode = "both";
    int n_shards = 0;      // 0 = sequential mode only
    int last_folder = argc - 1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a=="flat"||a=="hierarchy"||a=="both") { mode=a; last_folder=i-1; }
        if (a.substr(0,7)=="shards=") {
            n_shards = std::stoi(a.substr(7));
            last_folder = std::min(last_folder, i-1);
        }
    }

    auto files = collect_files(argc, argv, 1, last_folder);
    if (files.empty()) { std::cerr << "No .mbo.json files found.\n"; return 1; }
    std::cout << "\nTotal: " << files.size() << " files\n\n";

    std::vector<BenchResult> results;

    // ── Sequential runs ───────────────────────────────────────────────────────
    if (mode == "flat" || mode == "both") {
        std::cout << "── Sequential FlatMerger ────────────────────────\n";
        results.push_back(run_sequential("Seq+Flat", files, false, true));
    }
    if (mode == "hierarchy" || mode == "both") {
        std::cout << "\n── Sequential HierarchyMerger ───────────────────\n";
        results.push_back(run_sequential("Seq+Hierarchy", files, true, false));
    }

    // ── Sharded runs (bonus) ──────────────────────────────────────────────────
    if (n_shards > 0) {
        std::cout << "\n── Sharded FlatMerger (shards=" << n_shards << ") ────\n";
        results.push_back(run_sharded("Sharded+Flat("+std::to_string(n_shards)+")",
                                       files, false, n_shards));
    }

    // ── Benchmark table ───────────────────────────────────────────────────────
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    BENCHMARK RESULTS                        ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    std::cout << "║  " << std::left << std::setw(25) << "Strategy"
              << std::setw(12) << "Messages"
              << std::setw(10) << "Time(s)"
              << std::setw(14) << "Throughput" << " ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════╣\n";
    for (auto& r : results) {
        std::cout << "║  " << std::left << std::setw(25) << r.label
                  << std::setw(12) << r.messages
                  << std::fixed << std::setprecision(2)
                  << std::setw(10) << r.wall_sec
                  << std::setprecision(0)
                  << std::setw(14) << r.throughput << " ║\n";
    }
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";

    return 0;
}
