#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <thread>
#include <vector>

#include "MarketDataEvent.hpp"
#include "LimitOrderBook.hpp"
#include "FlatMerger.hpp"
#include "HierarchyMerger.hpp"
#include "ThreadSafeQueue.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static MarketDataEvent make_add(uint64_t order_id, Side side,
                                int64_t price, int64_t qty,
                                uint64_t ts = 0,
                                uint64_t iid = 1) {
    MarketDataEvent ev;
    ev.type          = EventType::Add;
    ev.ts            = ts;
    ev.order_id      = order_id;
    ev.instrument_id = iid;
    ev.side          = side;
    ev.price         = price;
    ev.qty           = qty;
    return ev;
}

static MarketDataEvent make_cancel(uint64_t order_id, int64_t qty = 0,
                                   uint64_t ts = 0) {
    MarketDataEvent ev;
    ev.type     = EventType::Cancel;
    ev.ts       = ts;
    ev.order_id = order_id;
    ev.qty      = qty;
    return ev;
}

static MarketDataEvent make_modify(uint64_t order_id, Side side,
                                   int64_t price, int64_t qty,
                                   uint64_t ts = 0) {
    MarketDataEvent ev;
    ev.type     = EventType::Modify;
    ev.ts       = ts;
    ev.order_id = order_id;
    ev.side     = side;
    ev.price    = price;
    ev.qty      = qty;
    return ev;
}

// ─────────────────────────────────────────────────────────────────────────────
// LimitOrderBook tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LOB: empty book has no best bid/ask", "[lob]") {
    LimitOrderBook lob;
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(!lob.best_ask().has_value());
    REQUIRE(lob.empty());
}

TEST_CASE("LOB: single bid Add", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    REQUIRE(lob.best_bid() == 100'000'000'000LL);
    REQUIRE(!lob.best_ask().has_value());
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 10);
}

TEST_CASE("LOB: single ask Add", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Ask, 101'000'000'000LL, 5));
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(lob.best_ask() == 101'000'000'000LL);
}

TEST_CASE("LOB: best bid is highest price", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 99'000'000'000LL,  5));
    lob.apply_event(make_add(2, Side::Bid, 100'000'000'000LL, 3));
    lob.apply_event(make_add(3, Side::Bid, 98'000'000'000LL,  7));
    REQUIRE(lob.best_bid() == 100'000'000'000LL);
}

TEST_CASE("LOB: best ask is lowest price", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Ask, 102'000'000'000LL, 5));
    lob.apply_event(make_add(2, Side::Ask, 101'000'000'000LL, 3));
    lob.apply_event(make_add(3, Side::Ask, 103'000'000'000LL, 7));
    REQUIRE(lob.best_ask() == 101'000'000'000LL);
}

TEST_CASE("LOB: Cancel removes order fully", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_cancel(1, 10));
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 0);
}

TEST_CASE("LOB: Cancel reduces order partially", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_cancel(1, 4));
    REQUIRE(lob.best_bid() == 100'000'000'000LL);
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 6);
}

TEST_CASE("LOB: Modify changes price and qty", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_modify(1, Side::Bid, 101'000'000'000LL, 5));
    REQUIRE(lob.best_bid() == 101'000'000'000LL);
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 0);
    REQUIRE(lob.volume_at_price(Side::Bid, 101'000'000'000LL) == 5);
}

TEST_CASE("LOB: two orders at same price level", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_add(2, Side::Bid, 100'000'000'000LL,  5));
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 15);
    lob.apply_event(make_cancel(1, 10));
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 5);
}

TEST_CASE("LOB: Reset clears everything", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_add(2, Side::Ask, 101'000'000'000LL,  5));
    MarketDataEvent reset_ev;
    reset_ev.type = EventType::Reset;
    lob.apply_event(reset_ev);
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(!lob.best_ask().has_value());
    REQUIRE(lob.empty());
    REQUIRE(lob.order_count() == 0);
}

TEST_CASE("LOB: Trade reduces order like Cancel", "[lob]") {
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Ask, 101'000'000'000LL, 10));
    MarketDataEvent trade;
    trade.type     = EventType::Trade;
    trade.order_id = 1;
    trade.qty      = 3;
    lob.apply_event(trade);
    REQUIRE(lob.volume_at_price(Side::Ask, 101'000'000'000LL) == 7);
}

TEST_CASE("LOB: Cancel of unknown order is no-op", "[lob]") {
    LimitOrderBook lob;
    REQUIRE_NOTHROW(lob.apply_event(make_cancel(999, 5)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Merger tests
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<MarketDataEvent> make_stream(std::vector<uint64_t> timestamps) {
    std::vector<MarketDataEvent> v;
    for (uint64_t ts : timestamps) {
        MarketDataEvent ev;
        ev.type = EventType::Add;
        ev.ts   = ts;
        v.push_back(ev);
    }
    return v;
}

template<typename Merger>
static std::vector<uint64_t> drain_timestamps(Merger& merger) {
    std::vector<uint64_t> out;
    MarketDataEvent ev;
    while (merger.next(ev)) out.push_back(ev.ts);
    return out;
}

TEST_CASE("FlatMerger: single stream comes out in order", "[merger][flat]") {
    FlatMerger m({ make_stream({1,2,3,4,5}) });
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1,2,3,4,5});
}

TEST_CASE("FlatMerger: two streams merge correctly", "[merger][flat]") {
    FlatMerger m({
        make_stream({1, 3, 5}),
        make_stream({2, 4, 6})
    });
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1,2,3,4,5,6});
}

TEST_CASE("FlatMerger: three streams", "[merger][flat]") {
    FlatMerger m({
        make_stream({1, 4, 7}),
        make_stream({2, 5, 8}),
        make_stream({3, 6, 9})
    });
    auto ts = drain_timestamps(m);
    std::vector<uint64_t> expected{1,2,3,4,5,6,7,8,9};
    REQUIRE(ts == expected);
}

TEST_CASE("FlatMerger: one empty stream", "[merger][flat]") {
    FlatMerger m({
        make_stream({1, 2, 3}),
        make_stream({})
    });
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1,2,3});
}

TEST_CASE("FlatMerger: all timestamps equal — all events come out", "[merger][flat]") {
    FlatMerger m({
        make_stream({5, 5, 5}),
        make_stream({5, 5})
    });
    auto ts = drain_timestamps(m);
    REQUIRE(ts.size() == 5);
    for (auto t : ts) REQUIRE(t == 5);
}

TEST_CASE("HierarchyMerger: single stream", "[merger][hierarchy]") {
    HierarchyMerger m({ make_stream({10,20,30}) });
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{10,20,30});
}

TEST_CASE("HierarchyMerger: two streams", "[merger][hierarchy]") {
    HierarchyMerger m({
        make_stream({1, 3, 5}),
        make_stream({2, 4, 6})
    });
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1,2,3,4,5,6});
}

TEST_CASE("HierarchyMerger: four streams", "[merger][hierarchy]") {
    HierarchyMerger m({
        make_stream({1, 5}),
        make_stream({2, 6}),
        make_stream({3, 7}),
        make_stream({4, 8})
    });
    auto ts = drain_timestamps(m);
    std::vector<uint64_t> expected{1,2,3,4,5,6,7,8};
    REQUIRE(ts == expected);
}

TEST_CASE("FlatMerger and HierarchyMerger agree on output", "[merger]") {
    auto streams_a = std::vector<std::vector<MarketDataEvent>>{
        make_stream({1, 4, 7, 10}),
        make_stream({2, 5, 8, 11}),
        make_stream({3, 6, 9, 12})
    };
    auto streams_b = streams_a;

    FlatMerger      flat(std::move(streams_a));
    HierarchyMerger hier(std::move(streams_b));

    auto ts_flat = drain_timestamps(flat);
    auto ts_hier = drain_timestamps(hier);
    REQUIRE(ts_flat == ts_hier);
}

// ─────────────────────────────────────────────────────────────────────────────
// ThreadSafeQueue tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Queue: push and pop single item", "[queue]") {
    ThreadSafeQueue<int> q(10);
    q.push(42);
    q.set_done();
    auto v = q.pop();
    REQUIRE(v.has_value());
    REQUIRE(*v == 42);
}

TEST_CASE("Queue: pop on empty+done returns nullopt", "[queue]") {
    ThreadSafeQueue<int> q(10);
    q.set_done();
    REQUIRE(!q.pop().has_value());
}

TEST_CASE("Queue: FIFO ordering", "[queue]") {
    ThreadSafeQueue<int> q(100);
    for (int i = 0; i < 5; ++i) q.push(i);
    q.set_done();
    for (int i = 0; i < 5; ++i) {
        auto v = q.pop();
        REQUIRE(v.has_value());
        REQUIRE(*v == i);
    }
}

TEST_CASE("Queue: producer/consumer threads", "[queue]") {
    ThreadSafeQueue<int> q(50);
    const int N = 1000;
    std::vector<int> results;
    results.reserve(N);

    std::thread producer([&]{
        for (int i = 0; i < N; ++i) q.push(i);
        q.set_done();
    });

    std::thread consumer([&]{
        while (auto v = q.pop()) results.push_back(*v);
    });

    producer.join();
    consumer.join();

    REQUIRE(results.size() == N);
    for (int i = 0; i < N; ++i) REQUIRE(results[i] == i);
}

// ─────────────────────────────────────────────────────────────────────────────
// MarketDataEvent helpers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent: price_decimal", "[event]") {
    MarketDataEvent ev;
    ev.price = 5'411'750'000'000LL;
    REQUIRE(ev.price_decimal() == Approx(5411.75));
}

TEST_CASE("MarketDataEvent: type_str", "[event]") {
    REQUIRE(std::string(MarketDataEvent::type_str(EventType::Add))    == "Add");
    REQUIRE(std::string(MarketDataEvent::type_str(EventType::Cancel)) == "Cancel");
    REQUIRE(std::string(MarketDataEvent::type_str(EventType::Reset))  == "Reset");
}


// ─────────────────────────────────────────────────────────────────────────────
// ShardedDispatcher tests
// ─────────────────────────────────────────────────────────────────────────────
#include "ShardedDispatcher.hpp"

TEST_CASE("ShardedDispatcher: all events processed", "[sharded]") {
    ThreadSafeQueue<MarketDataEvent> q(1000);
    ShardedDispatcher sd(q, 2);

    // Push 100 Add events across 4 instruments
    std::thread producer([&]{
        for (uint64_t i = 0; i < 100; ++i) {
            auto ev = make_add(i, Side::Bid, 100'000'000'000LL, 1, i, (i % 4) + 1);
            q.push(ev);
        }
        q.set_done();
    });

    sd.run();
    producer.join();

    REQUIRE(sd.total_events() == 100);
}

TEST_CASE("ShardedDispatcher: LOBs contain correct data", "[sharded]") {
    ThreadSafeQueue<MarketDataEvent> q(100);
    ShardedDispatcher sd(q, 2);

    // instrument_id=1 → worker 1%2=1, instrument_id=2 → worker 2%2=0
    std::thread producer([&]{
        q.push(make_add(1, Side::Bid, 100'000'000'000LL, 10, 1, /*iid=*/1));
        q.push(make_add(2, Side::Ask, 101'000'000'000LL,  5, 2, /*iid=*/2));
        q.set_done();
    });

    sd.run();
    producer.join();

    auto* lob1 = sd.get_lob(1);
    auto* lob2 = sd.get_lob(2);
    REQUIRE(lob1 != nullptr);
    REQUIRE(lob2 != nullptr);
    REQUIRE(lob1->best_bid() == 100'000'000'000LL);
    REQUIRE(lob2->best_ask() == 101'000'000'000LL);
}

TEST_CASE("ShardedDispatcher: worker count respected", "[sharded]") {
    ThreadSafeQueue<MarketDataEvent> q(100);
    ShardedDispatcher sd4(q, 4);
    REQUIRE(sd4.worker_count() == 4);

    // Just drain empty queue
    q.set_done();
    sd4.run();
    REQUIRE(sd4.total_events() == 0);
}

