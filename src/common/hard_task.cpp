/**
 * hard_task.cpp
 * -------------
 * Hard task : parallel multi-file ingestion with two merge strategies .
 *
 * Architecture
 * ────────────
 *  — N producer threads (one per file) parse NDJSON lines and push
 *    MarketDataEvent objects into per-file queues .
 *  — A single dispatcher thread reads from a merged , globally chronological
 *    stream and calls processMarketDataEvent() .
 *
 * Two merging strategies are implemented :
 *  1. FlatMerger   — classic k-way merge with a min-heap .
 *  2. HierarchyMerger – binary tournament—tree of blocking queues .
 *
 * Usage :
 *   ./hard_task <folder_path> [flat|hierarchy]
 *   (default : runs both and benchmarks them)
 *
 * Build :
 *   g++ -std=c++20 -O3 -I../include hard_task.cpp -o hard_task -lpthread
 */

#include "MarketDataEvent.hpp"
#include "NdjsonParser.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem ;

// ─────────────────────────────────────────────────────────────────────────────
//  THREAD—SAFE BOUNDED SPSC/MPSC QUEUE
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief A bounded , blocking multi-producer single-consumer queue .
 *
 * Producers call push() ; the consumer calls pop() which blocks until an item
 * is available .  When all producers are done they call set_done() , which causes
 * pop() to return std::nullopt once the queue is empty .
 */
template <typename T , size_t Capacity = 4096>
class BlockingQueue {
public:
    void push(T item) {
        std::unique_lock lock(mutex_) ;
        not_full_.wait(lock , [&]{ return queue_.size() < Capacity ; }) ;
        queue_.push(std::move(item)) ;
        not_empty_.notify_one() ;
    }

    std::optional<T> pop() {
        std::unique_lock lock(mutex_) ;
        not_empty_.wait(lock, [&]{ return !queue_.empty() || done_ ; }) ;
        if (queue_.empty()) return std::nullopt ;
        T item = std::move(queue_.front()) ;
        queue_.pop() ;
        not_full_.notify_one() ;
        return item ;
    }

    void set_done() {
        std::unique_lock lock(mutex_) ;
        done_ = true ;
        not_empty_.notify_all() ;
    }

    bool is_done() const {
        std::lock_guard lock(mutex_) ;
        return done_ && queue_.empty() ;
    }

private:
    mutable std::mutex      mutex_ ;
    std::condition_variable not_empty_ ;
    std::condition_variable not_full_ ;
    std::queue<T>           queue_ ;
    bool                    done_ = false ;
} ;

using EventQueue = BlockingQueue<MarketDataEvent , 8192> ;

// ─────────────────────────────────────────────────────────────────────────────
//  PRODUCER THREAD
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Reads one NDJSON file line—by—line and pushes events into a queue .
 * @param filepath  Path to the daily NDJSON file .
 * @param out_queue Target queue (shared with the merger) .
 * @param counter   Atomic counter incremented for each successfully parsed event .
 */
static void producer_thread(const std::string& filepath ,
                             std::shared_ptr<EventQueue> out_queue ,
                             std::atomic<uint64_t>& counter) {
    std::ifstream file(filepath) ;
    if (!file.is_open()) {
        std::cerr << "[Producer] Cannot open : " << filepath << "\n" ;
        out_queue->set_done() ;
        return ;
    }

    std::string line ;
    while (std::getline(file , line)) {
        if (line.empty()) continue ;
        auto opt = NdjsonParser::parse_line(line) ;
        if (opt) {
            out_queue->push(std::move(*opt)) ;
            ++counter ;
        }
    }
    out_queue->set_done() ;
}

// ─────────────────────────────────────────────────────────────────────────────
//  CONSUMER / DISPATCHER
// ─────────────────────────────────────────────────────────────────────────────

static void processMarketDataEvent(const MarketDataEvent& event) {
    // In the full backtester this dispatches to the LOB engine .
    // For now we count events ; verbose printing is optional .
    (void)event ;
    // Uncomment for verbose output :
    // std::cout << event << "\n" ;
}

// ─────────────────────────────────────────────────────────────────────────────
//  STRATEGY №1 : FLAT K—WAY MERGER
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Classic k—way merge using a min-heap .
 *
 * Holds the "current" pending event from each input queue . On each step it
 * picks the event with the smallest ts_recv , emits it , and pulls the next
 * from the corresponding queue .
 *
 * Time complexity : O(N log k) where N = total events , k = number of files .
 * Memory: O(k) heap entries.
 */
class FlatMerger {
public:
    explicit FlatMerger(std::vector<std::shared_ptr<EventQueue>> queues)
        : queues_(std::move(queues)) {}

    /**
     * @brief Run the merge and dispatch all events .
     * @param dispatcher  Called for every event in chronological order .
     * @return Total events dispatched .
     */
    uint64_t run(std::function<void(const MarketDataEvent&)> dispatcher) {
        // Min-heap : (event , queue_index)
        using Entry = std::pair<MarketDataEvent , size_t> ;
        auto cmp = [](const Entry& a , const Entry& b) {
            return a.first > b.first;   // min-heap by ts_recv
        } ;
        std::priority_queue<Entry , std::vector<Entry> , decltype(cmp)> heap(cmp) ;

        // Seed the heap with the first event from each queue
        for (size_t i = 0 ; i < queues_.size() ; ++i) {
            auto opt = queues_[i]->pop() ;
            if (opt) heap.push({std::move(*opt) , i}) ;
        }

        uint64_t count = 0 ;
        while (!heap.empty()) {
            auto [evt , idx] = heap.top() ;
            heap.pop() ;

            dispatcher(evt) ;
            ++count ;

            // Refill from the same queue
            auto next = queues_[idx]->pop() ;
            if (next) heap.push({std::move(*next) , idx}) ;
        }
        return count ;
    }

private:
    std::vector<std::shared_ptr<EventQueue>> queues_ ;
} ;

// ─────────────────────────────────────────────────────────────────────────────
//  STRATEGY №2 : HIERARCHY MERGER (BINARY TOURNAMENT TREE)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Merges two EventQueue streams into one BlockingQueue , in order .
 *
 * Each MergePair runs its own background thread that continuously picks the
 * chronologically earliest event from left/right and pushes it to the output .
 * Multiple MergePairs can be composed into a tree .
 */
class MergePair {
public:
    MergePair(std::shared_ptr<EventQueue> left ,
              std::shared_ptr<EventQueue> right)
        : left_(std::move(left)) ,
          right_(std::move(right)) ,
          output_(std::make_shared<EventQueue>()) {}

    std::shared_ptr<EventQueue> output() const { return output_ ; }

    /** @brief Start the background merge thread. */
    void start() {
        thread_ = std::thread([this]{ merge_loop() ; }) ;
    }

    void join() { if (thread_.joinable()) thread_.join() ; }

private:
    void merge_loop() {
        std::optional<MarketDataEvent> left_buf  = left_->pop() ;
        std::optional<MarketDataEvent> right_buf = right_->pop() ;

        while (left_buf || right_buf) {
            if (!left_buf) {
                output_->push(std::move(*right_buf)) ;
                right_buf = right_->pop() ;
            } else if (!right_buf) {
                output_->push(std::move(*left_buf)) ;
                left_buf = left_->pop() ;
            } else if (*left_buf > *right_buf) {
                // right is earlier
                output_->push(std::move(*right_buf)) ;
                right_buf = right_->pop() ;
            } else {
                output_->push(std::move(*left_buf)) ;
                left_buf = left_->pop() ;
            }
        }
        output_->set_done() ;
    }

    std::shared_ptr<EventQueue> left_ ;
    std::shared_ptr<EventQueue> right_ ;
    std::shared_ptr<EventQueue> output_ ;
    std::thread thread_ ;
} ;

/**
 * @brief Builds a binary merge tree from a list of per-file queues .
 *
 * Level 0 : original per-file queues
 * Level 1 : pair-wise MergePairs
 * Level 2 : pair-wise MergePairs of level—1 outputs
 * …until a single root output queue remains .
 */
class HierarchyMerger {
public:
    explicit HierarchyMerger(std::vector<std::shared_ptr<EventQueue>> queues)
        : leaf_queues_(std::move(queues)) {}

    uint64_t run(std::function<void(const MarketDataEvent&)> dispatcher) {
        // Build the tree
        std::vector<std::shared_ptr<EventQueue>> current = leaf_queues_ ;

        while (current.size() > 1) {
            std::vector<std::shared_ptr<EventQueue>> next_level ;
            for (size_t i = 0 ; i < current.size(); i += 2) {
                if (i + 1 < current.size()) {
                    auto pair = std::make_shared<MergePair>(current[i] , current[i+1]) ;
                    pair->start() ;
                    pairs_.push_back(pair) ;
                    next_level.push_back(pair->output()) ;
                } else {
                    // Odd one out – promote directly
                    next_level.push_back(current[i]) ;
                }
            }
            current = std::move(next_level) ;
        }

        // current[0] is the single root queue
        auto root = current[0] ;
        uint64_t count = 0 ;
        while (true) {
            auto opt = root->pop() ;
            if (!opt) break ;
            dispatcher(*opt) ;
            ++count ;
        }

        // Join all pair threads
        for (auto& p : pairs_) p->join() ;
        return count ;
    }

private:
    std::vector<std::shared_ptr<EventQueue>>  leaf_queues_ ;
    std::vector<std::shared_ptr<MergePair>>   pairs_ ;
} ;

// ─────────────────────────────────────────────────────────────────────────────
//  BENCHMARK HARNESS
// ─────────────────────────────────────────────────────────────────────────────

struct BenchResult {
    std::string strategy ;
    uint64_t    total_messages ;
    double      wall_seconds ;
    double      throughput_mps ;
} ;

static BenchResult run_benchmark(
    const std::string& strategy_name ,
    const std::vector<std::string>& files ,
    bool use_hierarchy)
{
    std::atomic<uint64_t> producer_count{0} ;
    std::vector<std::shared_ptr<EventQueue>> queues ;
    std::vector<std::thread> producers ;

    // Start producers
    for (const auto& f : files) {
        auto q = std::make_shared<EventQueue>() ;
        queues.push_back(q) ;
        producers.emplace_back(producer_thread , f , q , std::ref(producer_count)) ;
    }

    auto t0 = std::chrono::steady_clock::now() ;
    uint64_t dispatched = 0 ;

    if (use_hierarchy) {
        HierarchyMerger merger(queues) ;
        dispatched = merger.run(processMarketDataEvent) ;
    } else {
        FlatMerger merger(queues) ;
        dispatched = merger.run(processMarketDataEvent) ;
    }

    auto t1 = std::chrono::steady_clock::now() ;
    double elapsed = std::chrono::duration<double>(t1 - t0).count() ;

    for (auto& t : producers) t.join() ;

    return {
        strategy_name ,
        dispatched ,
        elapsed ,
        dispatched / elapsed
    } ;
}

static void print_result(const BenchResult& r) {
    std::cout << std::left
              << "  Strategy   : " << r.strategy        << "\n"
              << "  Messages   : " << r.total_messages  << "\n"
              << std::fixed << std::setprecision(3)
              << "  Wall time  : " << r.wall_seconds    << " s\n"
              << std::setprecision(0)
              << "  Throughput : " << r.throughput_mps  << " msg/s\n" ;
}

// ─────────────────────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <folder1> [folder2] ... [flat|hierarchy|both]\n" ;
        std::cerr << "Example: " << argv[0] << " ./options/ ./futures/ both\n" ;
        return 1 ;
    }

    // Last argument is mode if flat/hierarchy/both, otherwise default both
    std::string mode = "both" ;
    int last_folder_idx = argc - 1 ;
    {
        std::string last_arg = argv[argc - 1] ;
        if (last_arg == "flat" || last_arg == "hierarchy" || last_arg == "both") {
            mode = last_arg ;
            last_folder_idx = argc - 2 ;
        }
    }

    // Collect .mbo.json files from ALL folders given
    std::vector<std::string> files ;
    for (int i = 1 ; i <= last_folder_idx ; ++i) {
        const fs::path folder = argv[i] ;
        if (!fs::is_directory(folder)) {
            std::cerr << "Error: " << folder << " is not a directory.\n" ;
            return 1 ;
        }
        size_t count_before = files.size() ;
        for (const auto& entry : fs::directory_iterator(folder)) {
            std::string name = entry.path().filename().string() ;
            if (name.find(".mbo.json") != std::string::npos) {
                files.push_back(entry.path().string()) ;
            }
        }
        std::cout << "Folder " << i << ": " << folder.string()
                  << "  ->  " << (files.size() - count_before) << " files\n" ;
    }
    std::sort(files.begin(), files.end()) ;

    if (files.empty()) {
        std::cerr << "No .mbo.json files found.\n" ;
        return 1 ;
    }

    std::cout << "\nTotal: " << files.size() << " NDJSON files across "
              << last_folder_idx << " folder(s)\n\n" ;

    std::vector<BenchResult> results ;

    if (mode == "flat" || mode == "both") {
        std::cout << "── Running FlatMerger ──────────────────────────────\n" ;
        results.push_back(run_benchmark("FlatMerger (k-way heap)", files, false)) ;
        print_result(results.back()) ;
        std::cout << "\n" ;
    }

    if (mode == "hierarchy" || mode == "both") {
        std::cout << "── Running HierarchyMerger ─────────────────────────\n" ;
        results.push_back(run_benchmark("HierarchyMerger (binary tree)" , files, true)) ;
        print_result(results.back()) ;
        std::cout << "\n" ;
    }

    if (results.size() == 2) {
        double ratio = results[0].throughput_mps / results[1].throughput_mps ;
        std::cout << "── Comparison ──────────────────────────────────────\n" ;
        std::cout << std::fixed << std::setprecision(2)
                  << "  FlatMerger is " << ratio << "× "
                  << (ratio >= 1.0 ? "faster" : "slower")
                  << " than HierarchyMerger\n" ;
    }

    return 0 ;
}
