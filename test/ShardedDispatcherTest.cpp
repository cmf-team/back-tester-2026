// Tests for ShardedDispatcher: per-instrument order is preserved across
// workers, and cancel resolution works through the router-side cache.

#include "dispatcher/ShardedDispatcher.hpp"
#include "common/BasicTypes.hpp"
#include "market_data/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

#include <stdexcept>

using namespace cmf;

namespace
{

constexpr std::int64_t kPrice = 1'000'000'000LL;

MarketDataEvent makeEvent(MdAction action, OrderId order_id,
                          std::uint64_t iid, Side side, std::int64_t price,
                          std::uint32_t size)
{
    MarketDataEvent e;
    e.action = action;
    e.order_id = order_id;
    e.instrument_id = iid;
    e.side = side;
    e.price = price;
    e.size = size;
    return e;
}

} // namespace

TEST_CASE("ShardedDispatcher - rejects 0 / >kMaxWorkers", "[Sharded]")
{
    REQUIRE_THROWS_AS(ShardedDispatcher(0), std::invalid_argument);
    REQUIRE_THROWS_AS(ShardedDispatcher(ShardedDispatcher::kMaxWorkers + 1),
                      std::invalid_argument);
}

TEST_CASE("ShardedDispatcher - distributes events across workers", "[Sharded]")
{
    ShardedDispatcher d(4);
    // Use ids that span shards; the multiplicative hash spreads them.
    for (std::uint64_t iid = 1; iid <= 64; ++iid)
    {
        d.dispatch(makeEvent(MdAction::Add, iid, iid, Side::Buy, kPrice, 1));
    }
    const auto stats = d.finalize();
    REQUIRE(stats.events_total == 64);
    REQUIRE(stats.events_routed == 64);
    REQUIRE(stats.unresolved_iid == 0);
    // No worker is empty.
    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < 4; ++i)
    {
        REQUIRE(stats.per_worker_events[i] > 0);
        sum += stats.per_worker_events[i];
    }
    REQUIRE(sum == 64);
}

TEST_CASE("ShardedDispatcher - cancel without iid resolves via router cache",
          "[Sharded]")
{
    ShardedDispatcher d(2);
    d.dispatch(makeEvent(MdAction::Add, 1, 42, Side::Buy, kPrice, 5));
    d.dispatch(makeEvent(MdAction::Cancel, 1, 0, Side::None, 0, 5));
    const auto stats = d.finalize();
    REQUIRE(stats.events_routed == 2);
    REQUIRE(stats.unresolved_iid == 0);
}

TEST_CASE("ShardedDispatcher - per-instrument order preserved", "[Sharded]")
{
    ShardedDispatcher d(4);
    // Add 5 orders for the same iid, each with a distinct price; cancel
    // them in reverse order. Same shard, single worker — must not race.
    const std::uint64_t iid = 7;
    for (std::uint32_t i = 1; i <= 5; ++i)
    {
        d.dispatch(makeEvent(MdAction::Add, i, iid, Side::Buy,
                             kPrice * static_cast<std::int64_t>(i), i));
    }
    for (std::uint32_t i = 5; i >= 1; --i)
    {
        d.dispatch(makeEvent(MdAction::Cancel, i, 0, Side::None, 0, i));
        if (i == 1)
            break;
    }
    const auto stats = d.finalize();
    REQUIRE(stats.events_routed == 10);
    REQUIRE(stats.unresolved_iid == 0);
    // Aggregate orders_active across workers must be 0.
    std::uint64_t total_active = 0;
    for (auto v : stats.per_worker_orders_active)
        total_active += v;
    REQUIRE(total_active == 0);
}
