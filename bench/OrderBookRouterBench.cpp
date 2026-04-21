#include "common/MarketDataEvent.hpp"
#include "order_book/ShardedOrderBookRouter.hpp"
#include "order_book/SimpleOrderBookRouter.hpp"
#include <benchmark/benchmark.h>

#include <memory_resource>
#include <vector>

// Type aliases to avoid macro issues with commas in template args
using SimpleRouterBench = cmf::SimpleOrderBookRouter<>;
using ShardedRouterBench2 = cmf::ShardedOrderBookRouter<cmf::MapOrderBook, 2>;
using ShardedRouterBench4 = cmf::ShardedOrderBookRouter<cmf::MapOrderBook, 4>;

constexpr int64_t BASE = 5000'000'000'000LL;
constexpr int64_t TICK = 1'000'000'000LL;

static std::vector<cmf::MarketDataEvent>
make_router_events(int N, int num_instruments) {
  std::vector<cmf::MarketDataEvent> events;
  events.reserve(N);

  int add_count = 0;
  for (int i = 0; i < N; ++i) {
    cmf::MarketDataEvent e{};
    const int phase = i % 4;
    if (phase == 0 || phase == 1) {
      e.action = cmf::Action::Add;
      e.order_id = static_cast<uint64_t>(i + 1);
      e.instrument_id = static_cast<uint32_t>((i % num_instruments) + 1);
      e.side = (i % 2 == 0) ? cmf::Side::Buy : cmf::Side::Sell;
      int level = (i / 2) % 10;
      e.price = (e.side == cmf::Side::Buy)
                    ? BASE - static_cast<int64_t>(level) * TICK
                    : BASE + static_cast<int64_t>(level + 1) * TICK;
      e.size = static_cast<uint32_t>(10 + level);
      ++add_count;
    } else if (phase == 2 && add_count > 0) {
      e.action = cmf::Action::Cancel;
      e.order_id = static_cast<uint64_t>(i - 2 + 1);
      e.instrument_id = static_cast<uint32_t>(((i - 2) % num_instruments) + 1);
      e.size = 0;
    } else {
      e.action = cmf::Action::Modify;
      e.order_id = static_cast<uint64_t>(i / 4 * 4 + 1);
      e.instrument_id =
          static_cast<uint32_t>(((i / 4 * 4) % num_instruments) + 1);
      e.side = cmf::Side::Buy;
      e.price = BASE;
      e.size = 5;
    }
    events.push_back(e);
  }
  return events;
}

template <typename Factory>
static void BM_OrderBookRouter_Apply(benchmark::State &state) {
  const int N = static_cast<int>(state.range(0));
  const int num_instruments = static_cast<int>(state.range(1));
  const std::vector<cmf::MarketDataEvent> events =
      make_router_events(N, num_instruments);

  for (auto _ : state) {
    state.PauseTiming();
    auto router = Factory::create();
    state.ResumeTiming();

    for (const auto &e : events)
      router.apply(e);
    benchmark::DoNotOptimize(router);
  }
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(N));
}

// Factory methods for SimpleOrderBookRouter
struct SimpleRouterDefaultResourceFactory {
  static auto create() {
    return SimpleRouterBench(std::pmr::get_default_resource());
  }
};

struct SimpleRouterMonotonicPRFactory {
  static auto create() {
    static std::pmr::monotonic_buffer_resource pool;
    return SimpleRouterBench(&pool);
  }
};

// Factory methods for ShardedOrderBookRouter (uses default resource)
struct ShardedRouter2Factory {
  static auto create() { return ShardedRouterBench2(); }
};

struct ShardedRouter4Factory {
  static auto create() { return ShardedRouterBench4(); }
};

#define REGISTER_ROUTER_BENCH(Factory)                                         \
  BENCHMARK_TEMPLATE(BM_OrderBookRouter_Apply, Factory)                        \
      ->Args({1 << 20, 1 << 3})                                                \
      ->Unit(benchmark::kMicrosecond)                                          \
      ->Iterations(3)                                                          \
      ->Repetitions(2)                                                         \
      ->DisplayAggregatesOnly()

REGISTER_ROUTER_BENCH(SimpleRouterDefaultResourceFactory);
REGISTER_ROUTER_BENCH(SimpleRouterMonotonicPRFactory);
REGISTER_ROUTER_BENCH(ShardedRouter2Factory);
REGISTER_ROUTER_BENCH(ShardedRouter4Factory);
