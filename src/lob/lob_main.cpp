#include "LimitOrderBook.hpp"
#include "data_ingestion/SimpleLoader.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

static std::string nano_to_string(cmf::NanoTime ns) {
    std::time_t secs = ns / 1'000'000'000LL;
    long nanos       = ns % 1'000'000'000LL;
    std::tm t{};
    gmtime_r(&secs, &t);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec, nanos);
    return buf;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <ndjson-file> [snapshot_interval=100000]\n", argv[0]);
        return 1;
    }

    const std::uint64_t snapshot_interval =
        (argc >= 3) ? static_cast<std::uint64_t>(std::stoul(argv[2])) : 100'000;

    std::unordered_map<std::string, cmf::LimitOrderBook> books;
    std::unordered_map<std::string, std::uint64_t>        symbol_count;

    std::uint64_t total = 0;
    cmf::NanoTime first_ts = 0, last_ts = 0;

    const auto t_start = std::chrono::steady_clock::now();

    cmf::SimpleLoader loader(argv[1]);
    loader.load([&](const cmf::MarketDataEvent& ev) {
        if (cmf::flags::should_skip(ev.flags)) return;

        books[ev.symbol].apply(ev);
        ++symbol_count[ev.symbol];
        ++total;

        if (total == 1) first_ts = ev.ts_received;
        last_ts = ev.ts_received;

        if (total % snapshot_interval == 0) {
            // Find 3 busiest symbols so far
            std::vector<std::pair<std::uint64_t, std::string>> ranked;
            ranked.reserve(symbol_count.size());
            for (const auto& [sym, cnt] : symbol_count)
                ranked.emplace_back(cnt, sym);
            std::partial_sort(ranked.begin(),
                ranked.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(3, ranked.size())),
                ranked.end(),
                [](const auto& a, const auto& b){ return a.first > b.first; });

            std::printf("\n=== Snapshot at event %llu (%s) ===\n",
                static_cast<unsigned long long>(total),
                nano_to_string(ev.ts_received).c_str());

            const std::size_t top_n = std::min<std::size_t>(3, ranked.size());
            for (std::size_t i = 0; i < top_n; ++i) {
                const std::string& sym = ranked[i].second;
                const auto& book = books.at(sym);
                std::printf("[%s]  best_bid=%.9f  best_ask=%.9f\n",
                    sym.c_str(),
                    cmf::LimitOrderBook::unscale_price(book.best_bid_price()),
                    cmf::LimitOrderBook::unscale_price(book.best_ask_price()));
                book.print_snapshot(5);
            }
        }
    });

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    std::printf("\n=== Final best bid/ask per symbol ===\n");
    std::vector<std::pair<std::uint64_t, std::string>> ranked;
    ranked.reserve(symbol_count.size());
    for (const auto& [sym, cnt] : symbol_count)
        ranked.emplace_back(cnt, sym);
    std::sort(ranked.begin(), ranked.end(),
        [](const auto& a, const auto& b){ return a.first > b.first; });
    for (const auto& [cnt, sym] : ranked) {
        const auto& book = books.at(sym);
        std::printf("  %-30s  msgs=%llu  bid=%.9f  ask=%.9f\n",
            sym.c_str(),
            static_cast<unsigned long long>(cnt),
            cmf::LimitOrderBook::unscale_price(book.best_bid_price()),
            cmf::LimitOrderBook::unscale_price(book.best_ask_price()));
    }

    std::printf("\n=== Summary ===\n");
    std::printf("Total events : %llu\n",  static_cast<unsigned long long>(total));
    std::printf("Symbols      : %zu\n",   symbol_count.size());
    std::printf("First ts     : %s\n",    nano_to_string(first_ts).c_str());
    std::printf("Last  ts     : %s\n",    nano_to_string(last_ts).c_str());
    std::printf("Elapsed (s)  : %.3f\n",  elapsed);
    std::printf("Events/sec   : %llu\n",
        static_cast<unsigned long long>(static_cast<double>(total) / elapsed));

    return 0;
}
