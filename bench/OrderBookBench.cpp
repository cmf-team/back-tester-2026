#include "common/MarketDataEvent.hpp"
#include "order_book/AbseilOrderBook.hpp"
#include "order_book/MapOrderBook.hpp"
#include <benchmark/benchmark.h>

#include <vector>

static std::vector<cmf::MarketDataEvent> make_events(int N)
{
    std::vector<cmf::MarketDataEvent> events;
    events.reserve(N);

    // Price ladder: 10 distinct bid prices, 10 distinct ask prices
    constexpr int64_t BASE = 5000'000'000'000LL;
    constexpr int64_t TICK = 1'000'000'000LL;

    int add_count = 0;
    for (int i = 0; i < N; ++i)
    {
        cmf::MarketDataEvent e{};
        const int phase = i % 4;
        if (phase == 0 || phase == 1)
        {
            // Add
            e.action = cmf::Action::Add;
            e.order_id = static_cast<uint64_t>(i + 1);
            e.side = (i % 2 == 0) ? cmf::Side::Buy : cmf::Side::Sell;
            int level = (i / 2) % 10;
            e.price = (e.side == cmf::Side::Buy)
                          ? BASE - static_cast<int64_t>(level) * TICK
                          : BASE + static_cast<int64_t>(level + 1) * TICK;
            e.size = static_cast<uint32_t>(10 + level);
            ++add_count;
        }
        else if (phase == 2 && add_count > 0)
        {
            // Cancel of a previously added order
            e.action = cmf::Action::Cancel;
            e.order_id = static_cast<uint64_t>(i - 2 + 1);
            e.size = 0; // full cancel
        }
        else
        {
            // Modify size of an order still alive
            e.action = cmf::Action::Modify;
            e.order_id = static_cast<uint64_t>(i / 4 * 4 + 1);
            e.side = cmf::Side::Buy;
            e.price = BASE;
            e.size = 5;
        }
        events.push_back(e);
    }
    return events;
}

template <typename OrderBook>
static void BM_OrderBook_Apply(benchmark::State& state)
{
    const int N = static_cast<int>(state.range(0));
    const std::vector<cmf::MarketDataEvent> events = make_events(N);

    for (auto _ : state)
    {
        state.PauseTiming();
        OrderBook book;
        state.ResumeTiming();

        for (const auto& e : events)
            book.apply(e);
        benchmark::DoNotOptimize(book.best_price(cmf::Side::Buy));
    }
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(N));
}

#define REGISTER_OB_BENCH(Type)                  \
    BENCHMARK_TEMPLATE(BM_OrderBook_Apply, Type) \
        ->Arg(1 << 12)                           \
        ->Arg(1 << 14)                           \
        ->Arg(1 << 16)                           \
        ->Unit(benchmark::kMicrosecond)          \
        ->Iterations(5)                          \
        ->Repetitions(3)                         \
        ->DisplayAggregatesOnly()

REGISTER_OB_BENCH(cmf::MapOrderBook);
REGISTER_OB_BENCH(cmf::AbseilOrderBook);
