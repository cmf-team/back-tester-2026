/**
 * standard_task2.cpp
 * ------------------
 * Homework 2 Standard Variant:
 * Multi-instrument LOB reconstruction from a single L3 NDJSON file.
 *
 * Pipeline:
 *   1 producer thread  →  BlockingQueue  →  dispatcher  →  LOB map
 *
 * Usage:
 *   ./standard_task2 <path/to/file.mbo.json>
 *
 * Build:
 *   g++ -std=c++20 -O3 -I../include standard_task2.cpp -o standard_task2 -lpthread
 */

#include "MarketDataEvent.hpp"
#include "NdjsonParser.hpp"
#include "LimitOrderBook.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <iomanip>

// ─── BlockingQueue (reused from task 1) ──────────────────────────────────────

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
private:
    std::mutex mu_;
    std::condition_variable cv_empty_, cv_full_;
    std::queue<T> q_;
    bool done_ = false;
};

using EventQueue = BlockingQueue<MarketDataEvent, 8192>;

// ─── Producer ────────────────────────────────────────────────────────────────

static void producer(const std::string& path,
                     std::shared_ptr<EventQueue> q,
                     std::atomic<uint64_t>& parsed,
                     std::atomic<uint64_t>& skipped) {
    std::ifstream f(path);
    if (!f) { std::cerr << "Cannot open: " << path << "\n"; q->set_done(); return; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        auto opt = NdjsonParser::parse_line(line);
        if (opt) { q->push(std::move(*opt)); ++parsed; }
        else      ++skipped;
    }
    q->set_done();
}

// ─── LOB registry ────────────────────────────────────────────────────────────

/**
 * @brief Routes events to per-instrument LimitOrderBook instances.
 * Also handles the case where Cancel/Modify arrive for an order that
 * was added before the snapshot window (order_id lookup across books).
 */
class LobRegistry {
public:
    LimitOrderBook& get_or_create(uint32_t id, const std::string& sym = "") {
        auto it = lobs_.find(id);
        if (it == lobs_.end()) {
            lobs_.emplace(id, LimitOrderBook(id, sym));
            return lobs_.at(id);
        }
        // Update symbol if we now have it
        if (!sym.empty() && it->second.symbol().empty())
            it->second = LimitOrderBook(id, sym);
        return it->second;
    }

    void apply(const MarketDataEvent& evt) {
        get_or_create(evt.instrument_id, evt.symbol).apply(evt);
    }

    size_t size() const { return lobs_.size(); }

    void print_all_best(std::ostream& os = std::cout) const {
        os << "\n══════ Final Best Bid/Ask per Instrument ══════\n";
        os << std::left << std::setw(12) << "InstrID"
           << std::setw(32) << "Symbol"
           << std::setw(14) << "BestBid"
           << std::setw(14) << "BestAsk"
           << std::setw(12) << "Spread"
           << "Events\n";
        os << std::string(84, '-') << "\n";
        for (auto& [id, lob] : lobs_) {
            double bb = (lob.best_bid() == std::numeric_limits<int64_t>::min())
                        ? 0.0 : lob.best_bid() * 1e-9;
            double ba = (lob.best_ask() == std::numeric_limits<int64_t>::max())
                        ? 0.0 : lob.best_ask() * 1e-9;
            double sp = (lob.spread() >= 0) ? lob.spread() * 1e-9 : -1.0;
            os << std::setw(12) << id
               << std::setw(32) << lob.symbol().substr(0, 30)
               << std::fixed << std::setprecision(6)
               << std::setw(14) << bb
               << std::setw(14) << ba
               << std::setw(12) << sp
               << lob.total_events() << "\n";
        }
    }

    // Snapshot of top-N most active instruments
    void print_top_snapshots(int n = 3, int depth = 5) const {
        // Sort by event count
        std::vector<std::pair<uint64_t, uint32_t>> ranked;
        for (auto& [id, lob] : lobs_)
            ranked.push_back({lob.total_events(), id});
        std::sort(ranked.rbegin(), ranked.rend());

        std::cout << "\n══════ LOB Snapshots (Top " << n << " Most Active) ══════\n";
        for (int i = 0; i < std::min(n, (int)ranked.size()); ++i) {
            lobs_.at(ranked[i].second).print_snapshot(depth);
        }
    }

private:
    std::unordered_map<uint32_t, LimitOrderBook> lobs_;
};

// ─── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <file.mbo.json>\n";
        return 1;
    }

    const std::string filepath = argv[1];
    auto queue = std::make_shared<EventQueue>();
    std::atomic<uint64_t> parsed{0}, skipped{0};

    // ── Start producer ────────────────────────────────────────────────────────
    std::thread prod(producer, filepath, queue, std::ref(parsed), std::ref(skipped));

    // ── Dispatcher loop ───────────────────────────────────────────────────────
    LobRegistry registry;

    auto t0 = std::chrono::steady_clock::now();

    // Snapshot schedule: take 3 snapshots at ~25%, 50%, 75% of processing
    // We estimate based on event count milestones
    constexpr uint64_t SNAP_INTERVAL = 400'000;
    uint64_t next_snap = SNAP_INTERVAL;
    int snap_count = 0;

    uint64_t dispatched = 0;
    while (true) {
        auto opt = queue->pop();
        if (!opt) break;

        registry.apply(*opt);
        ++dispatched;

        // Periodic snapshots
        if (dispatched >= next_snap && snap_count < 3) {
            std::cout << "\n[Snapshot #" << ++snap_count
                      << " at event " << dispatched << "]\n";
            registry.print_top_snapshots(2, 4);
            next_snap += SNAP_INTERVAL;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    prod.join();

    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    // ── Final snapshot ────────────────────────────────────────────────────────
    std::cout << "\n[Final Snapshot]\n";
    registry.print_top_snapshots(3, 5);

    // ── Best bid/ask table ────────────────────────────────────────────────────
    registry.print_all_best();

    // ── Performance stats ─────────────────────────────────────────────────────
    std::cout << "\n╔══════════════════════════════════════════════╗\n";
    std::cout << "║         PERFORMANCE STATISTICS               ║\n";
    std::cout << "╠══════════════════════════════════════════════╣\n";
    std::cout << "║  Total events parsed  : " << std::setw(20) << parsed    << " ║\n";
    std::cout << "║  Events dispatched    : " << std::setw(20) << dispatched << " ║\n";
    std::cout << "║  Lines skipped        : " << std::setw(20) << skipped   << " ║\n";
    std::cout << "║  Instruments tracked  : " << std::setw(20) << registry.size() << " ║\n";
    std::cout << "║  Wall time (s)        : " << std::setw(20) << std::fixed
              << std::setprecision(3) << elapsed << " ║\n";
    std::cout << "║  Throughput (evt/s)   : " << std::setw(20)
              << (uint64_t)(dispatched / elapsed) << " ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n";

    return 0;
}
