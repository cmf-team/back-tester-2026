#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "common/LimitOrderBook.hpp"
#include "common/MarketDataEvent.hpp"
#include "common/PriceUtils.hpp"
#include "ingestion/SimpleDataParser.hpp"

using namespace cmf;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <json_l3_file>\n";
        return 1;
    }

    const std::string path = argv[1];

    LimitOrderBook book;
    SimpleDataParser parser(path);

    std::size_t totalEvents = 0;

    std::optional<NanoTime>   prev_ts_event;
    std::optional<std::uint32_t> prev_sequence;

    auto t0 = std::chrono::steady_clock::now();

    parser.parse_inner([&](const MarketDataEvent& ev) {
        // chronological order check
        if (prev_ts_event) {
            if (ev.ts_event < *prev_ts_event ||
               (ev.ts_event == *prev_ts_event && ev.sequence < *prev_sequence)) {
                std::cerr << "Non-monotonic (ts_event, sequence) at event #"
                          << totalEvents << "\n";
            }
        }
        prev_ts_event = ev.ts_event;
        prev_sequence = ev.sequence;

        book.apply(ev);
        ++totalEvents;

        if (totalEvents == 1'000 ||
            totalEvents == 100'000 ||
            totalEvents == 500'000) {
            std::cout << "\n--- Snapshot at event " << totalEvents << " ---\n";
            book.printSnapshot(10);
        }
    });

    auto t1 = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t1 - t0).count();
    double eps = seconds > 0.0 ? totalEvents / seconds : 0.0;

    std::cout << "Total events: " << totalEvents << "\n";
    std::cout << "Processing time: " << seconds << " s\n";
    std::cout << "Events/sec: " << eps << "\n\n";

    std::cout << "----- Final LOB Snapshot -----\n";
    book.printSnapshot(10);

    auto bestBid = book.bestBid();
    auto bestAsk = book.bestAsk();

    if (bestBid) {
        std::cout << "Best bid: price=" << formatScaledPrice(bestBid->first)
                  << " qty=" << bestBid->second << "\n";
    } else {
        std::cout << "Best bid: none\n";
    }

    if (bestAsk) {
        std::cout << "Best ask: price=" << formatScaledPrice(bestAsk->first)
                  << " qty=" << bestAsk->second << "\n";
    } else {
        std::cout << "Best ask: none\n";
    }

    return 0;
}