#include "common/MarketDataEvent.hpp"
#include "data_layer/EventFlatMerger.hpp"
#include "data_layer/EventHierarchyMerger.hpp"
#include "data_layer/Producer.hpp"
#include "main/LimitOrderBook.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace cmf;

static bool g_suppress_logs = true;

static void print_event(const MarketDataEvent& e) {
    std::printf(
        "ts_recv=%" PRId64 " ts_event=%" PRId64 " order_id=%" PRIu64 " side=%c price=%.9f size=%u action=%c sym=%s\n",
        e.ts_recv, e.ts_event, e.order_id, e.side, e.price, e.size, e.action, e.symbol
    );
}

struct Result {
    std::size_t count = 0;
    NanoTime first_ts = 0;
    NanoTime last_ts = 0;
};

enum class MergeMode {
    Flat,
    Hierarchy
};

enum class RunVariant {
    Standard,
    Hard
};

struct RuntimeOptions {
    RunVariant variant = RunVariant::Standard;
    MergeMode merge_mode = MergeMode::Flat;
    std::filesystem::path input;
};

struct SnapshotInstrument {
    uint32_t instrument_id = 0;
    LimitOrderBook::Bbo bbo{};
};

struct SnapshotTask {
    bool stop = false;
    std::size_t event_count = 0;
    uint32_t count = 0;
    std::array<SnapshotInstrument, 3> instruments{};
};

class SnapshotPrinterWorker {
public:
    void start() {
        thread_ = std::thread([this]() { run(); });
    }

    void enqueue(const SnapshotTask& task) {
        queue_.push(task);
    }

    void stop_and_join() {
        SnapshotTask stop_task{};
        stop_task.stop = true;
        queue_.push(stop_task);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    void run() {
        SnapshotTask task{};
        for (;;) {
            queue_.pop(task);
            if (task.stop) {
                break;
            }

            std::printf("\n=== SNAPSHOT @ event %zu ===\n", task.event_count);
            for (uint32_t i = 0; i < task.count; ++i) {
                const auto& s = task.instruments[i];
                std::printf(
                    "instrument=%u best_bid=%0.9f x %" PRId64 " | best_ask=%0.9f x %" PRId64 "\n",
                    s.instrument_id,
                    LimitOrderBook::to_double_price(s.bbo.bid_px),
                    s.bbo.bid_qty,
                    LimitOrderBook::to_double_price(s.bbo.ask_px),
                    s.bbo.ask_qty
                );
            }
        }
    }

    SpscQueue<SnapshotTask, 1024> queue_{};
    std::thread thread_{};
};

static bool is_book_update_action(char action) noexcept {
    return action == 'A' || action == 'C' || action == 'M' || action == 'T' || action == 'F';
}

static void apply_event_to_book(const MarketDataEvent& e, BookStore& books, OrderStore& orders) {
    if (!is_book_update_action(e.action)) return;

    const int64_t px = LimitOrderBook::to_scaled_price(e.price);
    const int64_t sz = static_cast<int64_t>(e.size);
    const auto ord_it = orders.find(e.order_id);

    switch (e.action) {
        case 'A': {
            LimitOrderBook::OrderState ord{};
            ord.instrument_id = e.instrument_id;
            ord.side = e.side;
            ord.price_scaled = px;
            ord.size = sz;
            books[e.instrument_id].apply_add(ord);
            orders[e.order_id] = ord;
            break;
        }
        case 'C':
        case 'T':
        case 'F': {
            if (ord_it == orders.end()) break;
            auto& ord = ord_it->second;
            books[ord.instrument_id].apply_trade_or_fill(ord, sz);
            ord.size -= std::min(ord.size, sz);
            if (ord.size <= 0) {
                orders.erase(ord_it);
            }
            break;
        }
        case 'M': {
            if (ord_it == orders.end()) break;
            auto& old_ord = ord_it->second;
            LimitOrderBook::OrderState new_ord = old_ord;
            if (px != LimitOrderBook::kInvalidPrice) {
                new_ord.price_scaled = px;
            }
            if (sz > 0) {
                new_ord.size = sz;
            }
            if (e.side == 'B' || e.side == 'A') {
                new_ord.side = e.side;
            }
            books[old_ord.instrument_id].apply_modify(old_ord, new_ord);
            old_ord = new_ord;
            break;
        }
        default:
            break;
    }
}

static void print_final_bbo(const BookStore& books) {
    std::printf("\n=== FINAL BBO ===\n");
    for (const auto& [instrument_id, book] : books) {
        const auto bbo = book.best_bid_ask();
        std::printf(
            "instrument=%u best_bid=%0.9f x %" PRId64 " | best_ask=%0.9f x %" PRId64 "\n",
            instrument_id,
            LimitOrderBook::to_double_price(bbo.bid_px),
            bbo.bid_qty,
            LimitOrderBook::to_double_price(bbo.ask_px),
            bbo.ask_qty
        );
    }
}

static void maybe_enqueue_snapshot(
    std::size_t event_count,
    const BookStore& books,
    const std::vector<std::size_t>& marks,
    std::size_t& next_mark,
    SnapshotPrinterWorker& worker
) {
    if (next_mark >= marks.size()) return;
    if (event_count < marks[next_mark]) return;

    SnapshotTask task{};
    task.event_count = event_count;
    for (const auto& [instrument_id, book] : books) {
        if (task.count >= task.instruments.size()) break;
        task.instruments[task.count].instrument_id = instrument_id;
        task.instruments[task.count].bbo = book.best_bid_ask();
        ++task.count;
    }

    worker.enqueue(task);
    ++next_mark;
}

template <typename Merger>
static Result run(Merger& merger,
                  std::vector<std::unique_ptr<Producer>>& producers)
{
    for (auto& p : producers) p->start();
    merger.start();

    Result r;
    BookStore books;
    books.reserve(128);
    OrderStore orders;
    orders.reserve(1 << 20);
    const std::vector<std::size_t> snapshot_marks{50'000, 250'000, 1'000'000};
    std::size_t next_snapshot = 0;
    SnapshotPrinterWorker snapshot_worker{};
    snapshot_worker.start();

    auto t0 = std::chrono::steady_clock::now();

    MarketDataEvent e;
    while (merger.next(e)) {
        if (!g_suppress_logs)
            print_event(e);

        if (r.count == 0)
            r.first_ts = e.ts_recv;

        r.last_ts = e.ts_recv;
        ++r.count;
        apply_event_to_book(e, books, orders);
        maybe_enqueue_snapshot(r.count, books, snapshot_marks, next_snapshot, snapshot_worker);
    }

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();

    for (auto& p : producers) p->join();
    merger.join();

    std::printf("\n=== RESULT ===\n");
    std::printf("Total messages : %zu\n", r.count);
    std::printf("Wall time (s)  : %.6f\n", sec);
    std::printf("Throughput     : %.0f msg/s\n", r.count / sec);
    std::printf("Books tracked  : %zu\n", books.size());
    std::printf("Active orders  : %zu\n", orders.size());
    snapshot_worker.stop_and_join();
    print_final_bbo(books);

    return r;
}

static std::vector<std::filesystem::path>
collect_files(const std::filesystem::path& input)
{
    std::vector<std::filesystem::path> files;

    if (std::filesystem::is_regular_file(input)) {
        files.push_back(input);
        return files;
    }

    if (std::filesystem::is_directory(input)) {
        for (auto& entry : std::filesystem::directory_iterator(input)) {
            auto p = entry.path();
            if (p.string().ends_with(".mbo.json"))
                files.push_back(p);
        }
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

static RuntimeOptions parse_args(int argc, char** argv) {
    RuntimeOptions opts{};
    opts.input = "test_data";

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--mode=hard") {
            opts.variant = RunVariant::Hard;
        } else if (arg == "--mode=standard") {
            opts.variant = RunVariant::Standard;
        } else if (arg == "--merger=hierarchy") {
            opts.merge_mode = MergeMode::Hierarchy;
        } else if (arg == "--merger=flat") {
            opts.merge_mode = MergeMode::Flat;
        } else if (arg == "--verbose") {
            g_suppress_logs = false;
        } else {
            opts.input = std::filesystem::path(arg);
        }
    }

    return opts;
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

int main(int argc, char** argv) {
    const RuntimeOptions opts = parse_args(argc, argv);
    auto files = collect_files(opts.input);

    if (files.empty()) {
        std::printf("No input files in %s\n", opts.input.c_str());
        return 1;
    }

    std::printf("Files loaded: %zu\n", files.size());
    std::printf(
        "Variant      : %s\n",
        opts.variant == RunVariant::Standard ? "standard" : "hard"
    );
    std::printf(
        "Merger       : %s\n",
        opts.merge_mode == MergeMode::Flat ? "flat" : "hierarchy"
    );

    StreamList streams;
    if (opts.variant == RunVariant::Standard) {
        streams.push_back({files.front()});
    } else {
        streams = build_streams(files);
    }

    Pipeline p;
    p.build(streams);

    if (opts.merge_mode == MergeMode::Flat) {
        EventFlatMerger merger(p.ptrs);
        run(merger, p.producers);
    } else {
        EventHierarchyMerger merger(p.ptrs);
        run(merger, p.producers);
    }

    return 0;
}