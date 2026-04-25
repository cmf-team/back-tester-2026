// Event-driven back-tester: single-file NDJSON → per-instrument LOB dispatcher.

#include "common/LimitOrderBook.hpp"
#include "common/MarketDataEvent.hpp"
#include "common/MarketDataParser.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// Print a snapshot for every tracked instrument.
static void printAllBooks(
    const std::unordered_map<uint32_t, LimitOrderBook>& books,
    std::size_t event_count)
{
    std::cout << "\n[Snapshot at event " << event_count << "]\n";
    for (const auto& [id, book] : books)
        book.printSnapshot(id, 5);
}

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: back-tester <path_to_ndjson_file>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open())
    {
        std::cerr << "Error: could not open " << argv[1] << "\n";
        return 1;
    }

    // ── Book registry ─────────────────────────────────────────────────────────
    // One LimitOrderBook per instrument_id, created on first event.
    std::unordered_map<uint32_t, LimitOrderBook> books;

    // ── Snapshot schedule ────────────────────────────────────────────────────
    // Print an intermediate snapshot at each of these event counts.
    const std::vector<std::size_t> snapshot_at = {10'000, 100'000, 500'000};
    std::size_t next_snapshot_idx = 0;

    // ── Processing loop (single producer — one file, already sorted) ─────────
    std::size_t total_events = 0;
    std::size_t skipped = 0;

    const auto wall_start = std::chrono::steady_clock::now();

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        auto ev_opt = parseNDJSON(line);
        if (!ev_opt)
        {
            ++skipped;
            continue;
        }

        // Route event to the correct book (create on first sight).
        const MarketDataEvent& ev = *ev_opt;
        books[ev.instrument_id].applyEvent(ev);
        ++total_events;

        // Intermediate snapshots.
        if (next_snapshot_idx < snapshot_at.size() &&
            total_events == snapshot_at[next_snapshot_idx])
        {
            printAllBooks(books, total_events);
            ++next_snapshot_idx;
        }
    }

    const auto wall_end = std::chrono::steady_clock::now();
    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start).count();

    // ── Final snapshots ───────────────────────────────────────────────────────
    printAllBooks(books, total_events);

    // ── Final best bid/ask per instrument ────────────────────────────────────
    std::cout << "\n--- Final Best Bid/Ask ---\n";
    std::cout << std::fixed << std::setprecision(9);
    for (const auto& [id, book] : books)
    {
        std::cout << "  instrument_id=" << std::setw(8) << id << ":";
        const auto bb = book.bestBid();
        const auto ba = book.bestAsk();
        if (bb)
            std::cout << "  bid=" << MarketDataEvent::priceToDouble(bb->first)
                      << " x " << bb->second;
        else
            std::cout << "  bid=N/A";
        if (ba)
            std::cout << "  ask=" << MarketDataEvent::priceToDouble(ba->first)
                      << " x " << ba->second;
        else
            std::cout << "  ask=N/A";
        std::cout << "\n";
    }

    // ── Performance statistics ────────────────────────────────────────────────
    const double elapsed_s = static_cast<double>(elapsed_ns) * 1e-9;
    const double events_per_sec = elapsed_s > 0 ? total_events / elapsed_s : 0.0;

    std::cout << "\n--- Performance ---\n";
    std::cout << "  Total events:  " << total_events << "\n";
    std::cout << "  Skipped lines: " << skipped << "\n";
    std::cout << "  Instruments:   " << books.size() << "\n";
    std::cout << "  Elapsed:       " << std::fixed << std::setprecision(3)
              << elapsed_s << " s\n";
    std::cout << "  Throughput:    " << std::fixed << std::setprecision(0)
              << events_per_sec << " events/s\n";

    return 0;
}
