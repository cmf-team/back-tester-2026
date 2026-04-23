#include "SimpleLoader.hpp"
#include <chrono>
#include <cstdio>
#include <ctime>
#include <deque>
#include <iostream>

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

static void processMarketDataEvent(const cmf::MarketDataEvent& ev) {
    std::cout << "ts_event, nanosec=" << ev.ts_event << " ts_event, usual=" << nano_to_string(ev.ts_event) 
              << " oid="    << ev.order_id
              << " side="   << cmf::side::to_string(ev.side)
              << " price="  << ev.price
              << " size="   << ev.qty
              << " action=" << cmf::action::to_string(ev.action)
              << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <ndjson-file>\n";
        return 1;
    }

    cmf::SimpleLoader loader(argv[1]);

    std::uint64_t total = 0;
    cmf::NanoTime first_ts = 0, last_ts = 0;
    std::vector<cmf::MarketDataEvent> first10;
    std::deque<cmf::MarketDataEvent>  last10;

    const auto t_start = std::chrono::steady_clock::now();
    loader.load([&](const cmf::MarketDataEvent& ev) {
        if (cmf::flags::should_skip(ev.flags)) return;

        ++total;
        if (total == 1) first_ts = ev.ts_received;
        last_ts = ev.ts_received;

        if (first10.size() < 10) first10.push_back(ev);
        last10.push_back(ev);
        if (last10.size() > 10) last10.pop_front();
    });

    std::cout << "--- First 10 events ---\n";
    for (const auto& ev : first10) processMarketDataEvent(ev);

    std::cout << "\n--- Last 10 events ---\n";
    for (const auto& ev : last10) processMarketDataEvent(ev);

    std::cout << "\n--- Summary ---\n";
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_start).count();

    std::cout << "Total messages   : " << total << "\n";
    std::cout << "First ts received: " << nano_to_string(first_ts) << "\n";
    std::cout << "Last  ts received: " << nano_to_string(last_ts)  << "\n";
    std::cout << "Elapsed (s)      : " << elapsed  << "\n";
    std::cout << "Msg/sec          : " << static_cast<std::uint64_t>(total / elapsed) << "\n";

    return 0;
}
