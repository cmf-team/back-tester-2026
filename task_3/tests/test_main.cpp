#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include <thread>
#include <vector>

#include "FlatMerger.hpp"
#include "HierarchyMerger.hpp"
#include "LimitOrderBook.hpp"
#include "MarketDataEvent.hpp"
#include "ThreadSafeQueue.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static MarketDataEvent make_add(uint64_t order_id, Side side,
                                int64_t price, int64_t qty,
                                uint64_t ts = 0,
                                uint64_t iid = 1)
{
    MarketDataEvent ev;
    ev.type = EventType::Add;
    ev.ts = ts;
    ev.order_id = order_id;
    ev.instrument_id = iid;
    ev.side = side;
    ev.price = price;
    ev.qty = qty;
    return ev;
}

static MarketDataEvent make_cancel(uint64_t order_id, int64_t qty = 0,
                                   uint64_t ts = 0)
{
    MarketDataEvent ev;
    ev.type = EventType::Cancel;
    ev.ts = ts;
    ev.order_id = order_id;
    ev.qty = qty;
    return ev;
}

static MarketDataEvent make_modify(uint64_t order_id, Side side,
                                   int64_t price, int64_t qty,
                                   uint64_t ts = 0)
{
    MarketDataEvent ev;
    ev.type = EventType::Modify;
    ev.ts = ts;
    ev.order_id = order_id;
    ev.side = side;
    ev.price = price;
    ev.qty = qty;
    return ev;
}

// ─────────────────────────────────────────────────────────────────────────────
// LimitOrderBook tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("LOB: empty book has no best bid/ask", "[lob]")
{
    LimitOrderBook lob;
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(!lob.best_ask().has_value());
    REQUIRE(lob.empty());
}

TEST_CASE("LOB: single bid Add", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    REQUIRE(lob.best_bid() == 100'000'000'000LL);
    REQUIRE(!lob.best_ask().has_value());
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 10);
}

TEST_CASE("LOB: single ask Add", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Ask, 101'000'000'000LL, 5));
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(lob.best_ask() == 101'000'000'000LL);
}

TEST_CASE("LOB: best bid is highest price", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 99'000'000'000LL, 5));
    lob.apply_event(make_add(2, Side::Bid, 100'000'000'000LL, 3));
    lob.apply_event(make_add(3, Side::Bid, 98'000'000'000LL, 7));
    REQUIRE(lob.best_bid() == 100'000'000'000LL);
}

TEST_CASE("LOB: best ask is lowest price", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Ask, 102'000'000'000LL, 5));
    lob.apply_event(make_add(2, Side::Ask, 101'000'000'000LL, 3));
    lob.apply_event(make_add(3, Side::Ask, 103'000'000'000LL, 7));
    REQUIRE(lob.best_ask() == 101'000'000'000LL);
}

TEST_CASE("LOB: Cancel removes order fully", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_cancel(1, 10));
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 0);
}

TEST_CASE("LOB: Cancel reduces order partially", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_cancel(1, 4));
    REQUIRE(lob.best_bid() == 100'000'000'000LL);
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 6);
}

TEST_CASE("LOB: Modify changes price and qty", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_modify(1, Side::Bid, 101'000'000'000LL, 5));
    REQUIRE(lob.best_bid() == 101'000'000'000LL);
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 0);
    REQUIRE(lob.volume_at_price(Side::Bid, 101'000'000'000LL) == 5);
}

TEST_CASE("LOB: two orders at same price level", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_add(2, Side::Bid, 100'000'000'000LL, 5));
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 15);
    lob.apply_event(make_cancel(1, 10));
    REQUIRE(lob.volume_at_price(Side::Bid, 100'000'000'000LL) == 5);
}

TEST_CASE("LOB: Reset clears everything", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Bid, 100'000'000'000LL, 10));
    lob.apply_event(make_add(2, Side::Ask, 101'000'000'000LL, 5));
    MarketDataEvent reset_ev;
    reset_ev.type = EventType::Reset;
    lob.apply_event(reset_ev);
    REQUIRE(!lob.best_bid().has_value());
    REQUIRE(!lob.best_ask().has_value());
    REQUIRE(lob.empty());
    REQUIRE(lob.order_count() == 0);
}

TEST_CASE("LOB: Trade reduces order like Cancel", "[lob]")
{
    LimitOrderBook lob;
    lob.apply_event(make_add(1, Side::Ask, 101'000'000'000LL, 10));
    MarketDataEvent trade;
    trade.type = EventType::Trade;
    trade.order_id = 1;
    trade.qty = 3;
    lob.apply_event(trade);
    REQUIRE(lob.volume_at_price(Side::Ask, 101'000'000'000LL) == 7);
}

TEST_CASE("LOB: Cancel of unknown order is no-op", "[lob]")
{
    LimitOrderBook lob;
    REQUIRE_NOTHROW(lob.apply_event(make_cancel(999, 5)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Merger tests
// ─────────────────────────────────────────────────────────────────────────────

static std::vector<MarketDataEvent> make_stream(std::vector<uint64_t> timestamps)
{
    std::vector<MarketDataEvent> v;
    for (uint64_t ts : timestamps)
    {
        MarketDataEvent ev;
        ev.type = EventType::Add;
        ev.ts = ts;
        v.push_back(ev);
    }
    return v;
}

template <typename Merger>
static std::vector<uint64_t> drain_timestamps(Merger& merger)
{
    std::vector<uint64_t> out;
    MarketDataEvent ev;
    while (merger.next(ev))
        out.push_back(ev.ts);
    return out;
}

TEST_CASE("FlatMerger: single stream comes out in order", "[merger][flat]")
{
    FlatMerger m({make_stream({1, 2, 3, 4, 5})});
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1, 2, 3, 4, 5});
}

TEST_CASE("FlatMerger: two streams merge correctly", "[merger][flat]")
{
    FlatMerger m({make_stream({1, 3, 5}),
                  make_stream({2, 4, 6})});
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("FlatMerger: three streams", "[merger][flat]")
{
    FlatMerger m({make_stream({1, 4, 7}),
                  make_stream({2, 5, 8}),
                  make_stream({3, 6, 9})});
    auto ts = drain_timestamps(m);
    std::vector<uint64_t> expected{1, 2, 3, 4, 5, 6, 7, 8, 9};
    REQUIRE(ts == expected);
}

TEST_CASE("FlatMerger: one empty stream", "[merger][flat]")
{
    FlatMerger m({make_stream({1, 2, 3}),
                  make_stream({})});
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1, 2, 3});
}

TEST_CASE("FlatMerger: all timestamps equal — all events come out", "[merger][flat]")
{
    FlatMerger m({make_stream({5, 5, 5}),
                  make_stream({5, 5})});
    auto ts = drain_timestamps(m);
    REQUIRE(ts.size() == 5);
    for (auto t : ts)
        REQUIRE(t == 5);
}

TEST_CASE("HierarchyMerger: single stream", "[merger][hierarchy]")
{
    HierarchyMerger m({make_stream({10, 20, 30})});
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{10, 20, 30});
}

TEST_CASE("HierarchyMerger: two streams", "[merger][hierarchy]")
{
    HierarchyMerger m({make_stream({1, 3, 5}),
                       make_stream({2, 4, 6})});
    auto ts = drain_timestamps(m);
    REQUIRE(ts == std::vector<uint64_t>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("HierarchyMerger: four streams", "[merger][hierarchy]")
{
    HierarchyMerger m({make_stream({1, 5}),
                       make_stream({2, 6}),
                       make_stream({3, 7}),
                       make_stream({4, 8})});
    auto ts = drain_timestamps(m);
    std::vector<uint64_t> expected{1, 2, 3, 4, 5, 6, 7, 8};
    REQUIRE(ts == expected);
}

TEST_CASE("FlatMerger and HierarchyMerger agree on output", "[merger]")
{
    auto streams_a = std::vector<std::vector<MarketDataEvent>>{
        make_stream({1, 4, 7, 10}),
        make_stream({2, 5, 8, 11}),
        make_stream({3, 6, 9, 12})};
    auto streams_b = streams_a;

    FlatMerger flat(std::move(streams_a));
    HierarchyMerger hier(std::move(streams_b));

    auto ts_flat = drain_timestamps(flat);
    auto ts_hier = drain_timestamps(hier);
    REQUIRE(ts_flat == ts_hier);
}

// ─────────────────────────────────────────────────────────────────────────────
// ThreadSafeQueue tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Queue: push and pop single item", "[queue]")
{
    ThreadSafeQueue<int> q(10);
    q.push(42);
    q.set_done();
    auto v = q.pop();
    REQUIRE(v.has_value());
    REQUIRE(*v == 42);
}

TEST_CASE("Queue: pop on empty+done returns nullopt", "[queue]")
{
    ThreadSafeQueue<int> q(10);
    q.set_done();
    REQUIRE(!q.pop().has_value());
}

TEST_CASE("Queue: FIFO ordering", "[queue]")
{
    ThreadSafeQueue<int> q(100);
    for (int i = 0; i < 5; ++i)
        q.push(i);
    q.set_done();
    for (int i = 0; i < 5; ++i)
    {
        auto v = q.pop();
        REQUIRE(v.has_value());
        REQUIRE(*v == i);
    }
}

TEST_CASE("Queue: producer/consumer threads", "[queue]")
{
    ThreadSafeQueue<int> q(50);
    const int N = 1000;
    std::vector<int> results;
    results.reserve(N);

    std::thread producer([&]
                         {
        for (int i = 0; i < N; ++i) q.push(i);
        q.set_done(); });

    std::thread consumer([&]
                         {
        while (auto v = q.pop()) results.push_back(*v); });

    producer.join();
    consumer.join();

    REQUIRE(results.size() == N);
    for (int i = 0; i < N; ++i)
        REQUIRE(results[i] == i);
}

// ─────────────────────────────────────────────────────────────────────────────
// MarketDataEvent helpers
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent: price_decimal", "[event]")
{
    MarketDataEvent ev;
    ev.price = 5'411'750'000'000LL;
    REQUIRE(ev.price_decimal() == Approx(5411.75));
}

TEST_CASE("MarketDataEvent: type_str", "[event]")
{
    REQUIRE(std::string(MarketDataEvent::type_str(EventType::Add)) == "Add");
    REQUIRE(std::string(MarketDataEvent::type_str(EventType::Cancel)) == "Cancel");
    REQUIRE(std::string(MarketDataEvent::type_str(EventType::Reset)) == "Reset");
}

// ─────────────────────────────────────────────────────────────────────────────
// ShardedDispatcher tests
// ─────────────────────────────────────────────────────────────────────────────
#include "ShardedDispatcher.hpp"

TEST_CASE("ShardedDispatcher: all events processed", "[sharded]")
{
    ThreadSafeQueue<MarketDataEvent> q(1000);
    ShardedDispatcher sd(q, 2);

    // Push 100 Add events across 4 instruments
    std::thread producer([&]
                         {
        for (uint64_t i = 0; i < 100; ++i) {
            auto ev = make_add(i, Side::Bid, 100'000'000'000LL, 1, i, (i % 4) + 1);
            q.push(ev);
        }
        q.set_done(); });

    sd.run();
    producer.join();

    REQUIRE(sd.total_events() == 100);
}

TEST_CASE("ShardedDispatcher: LOBs contain correct data", "[sharded]")
{
    ThreadSafeQueue<MarketDataEvent> q(100);
    ShardedDispatcher sd(q, 2);

    // instrument_id=1 → worker 1%2=1, instrument_id=2 → worker 2%2=0
    std::thread producer([&]
                         {
        q.push(make_add(1, Side::Bid, 100'000'000'000LL, 10, 1, /*iid=*/1));
        q.push(make_add(2, Side::Ask, 101'000'000'000LL,  5, 2, /*iid=*/2));
        q.set_done(); });

    sd.run();
    producer.join();

    auto* lob1 = sd.get_lob(1);
    auto* lob2 = sd.get_lob(2);
    REQUIRE(lob1 != nullptr);
    REQUIRE(lob2 != nullptr);
    REQUIRE(lob1->best_bid() == 100'000'000'000LL);
    REQUIRE(lob2->best_ask() == 101'000'000'000LL);
}

TEST_CASE("ShardedDispatcher: worker count respected", "[sharded]")
{
    ThreadSafeQueue<MarketDataEvent> q(100);
    ShardedDispatcher sd4(q, 4);
    REQUIRE(sd4.worker_count() == 4);

    // Just drain empty queue
    q.set_done();
    sd4.run();
    REQUIRE(sd4.total_events() == 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Strategy tests
// ─────────────────────────────────────────────────────────────────────────────
#include "AvellanedaStoikov.hpp"
#include "DataReader.hpp"
#include "Metrics.hpp"
#include "MicropriceAS.hpp"
#include "OrderManager.hpp"

// Helper: make a simple book snapshot
static BookSnapshot make_snap(double bid, double ask,
                              double bid_vol, double ask_vol,
                              uint64_t ts = 1000000000ULL)
{
    BookSnapshot s;
    s.timestamp = ts;
    s.bids.resize(1);
    s.bids[0] = {bid, bid_vol};
    s.asks.resize(1);
    s.asks[0] = {ask, ask_vol};
    return s;
}

// Helper: make a trade
static Trade make_trade(bool is_sell, double price, double amount,
                        uint64_t ts = 2000000000ULL)
{
    return Trade{ts, is_sell, price, amount};
}

TEST_CASE("OrderManager: place and cancel", "[strategy]")
{
    OrderManager om;
    om.place(1.0, 1.1, 100.0);
    REQUIRE(om.bid_order.has_value());
    REQUIRE(om.ask_order.has_value());
    REQUIRE(om.bid_order->price == Approx(1.0));
    REQUIRE(om.ask_order->price == Approx(1.1));

    om.cancel_all();
    REQUIRE_FALSE(om.bid_order.has_value());
    REQUIRE_FALSE(om.ask_order.has_value());
}

TEST_CASE("OrderManager: bid fill on sell trade", "[strategy]")
{
    OrderManager om;
    om.place(1.0, 1.1, 100.0);

    // Sell trade at 1.0 should fill our bid
    REQUIRE(om.check_bid_fill(1.0, true));
    REQUIRE_FALSE(om.check_ask_fill(1.0, true));

    om.fill_bid();
    REQUIRE_FALSE(om.bid_order->active);
}

TEST_CASE("OrderManager: ask fill on buy trade", "[strategy]")
{
    OrderManager om;
    om.place(1.0, 1.1, 100.0);

    // Buy trade at 1.1 should fill our ask
    REQUIRE(om.check_ask_fill(1.1, false));
    REQUIRE_FALSE(om.check_bid_fill(1.1, false));

    om.fill_ask();
    REQUIRE_FALSE(om.ask_order->active);
}

TEST_CASE("OrderManager: no fill when price doesnt cross", "[strategy]")
{
    OrderManager om;
    om.place(1.0, 1.1, 100.0);

    // Sell at 1.05 — above our bid, no fill
    REQUIRE_FALSE(om.check_bid_fill(1.05, true));
    // Buy at 1.05 — below our ask, no fill
    REQUIRE_FALSE(om.check_ask_fill(1.05, false));
}

TEST_CASE("Metrics: PnL calculation", "[strategy]")
{
    Metrics m;
    double mid = 1.05;

    // Buy 100 at 1.0
    Fill f1{1000, true, 1.0, 100.0};
    m.on_fill(f1, mid);

    REQUIRE(m.inventory == Approx(100.0));
    REQUIRE(m.turnover == Approx(100.0));
    // PnL = cash + inventory * mid = -100 + 100*1.05 = +5
    REQUIRE(m.pnl(mid) == Approx(5.0));

    // Sell 100 at 1.1
    Fill f2{2000, false, 1.1, 100.0};
    m.on_fill(f2, mid);

    REQUIRE(m.inventory == Approx(0.0));
    REQUIRE(m.turnover == Approx(210.0));
    // PnL = -100 + 110 + 0*mid = +10
    REQUIRE(m.pnl(mid) == Approx(10.0));
}

TEST_CASE("Metrics: inventory tracking", "[strategy]")
{
    Metrics m;
    double mid = 1.0;

    Fill buy{1000, true, 1.0, 50.0};
    Fill sell{2000, false, 1.0, 30.0};

    m.on_fill(buy, mid);
    REQUIRE(m.inventory == Approx(50.0));

    m.on_fill(sell, mid);
    REQUIRE(m.inventory == Approx(20.0));

    REQUIRE(m.num_fills == 2);
}

TEST_CASE("BookSnapshot: mid and spread", "[strategy]")
{
    auto snap = make_snap(1.0, 1.1, 100.0, 200.0);
    REQUIRE(snap.best_bid() == Approx(1.0));
    REQUIRE(snap.best_ask() == Approx(1.1));
    REQUIRE(snap.mid() == Approx(1.05));
    REQUIRE(snap.spread() == Approx(0.1));
}

TEST_CASE("AvellanedaStoikov: places bid below ask", "[strategy]")
{
    ASConfig cfg;
    cfg.gamma = 0.01;
    cfg.kappa = 1.5;
    cfg.T = 1.0;
    cfg.q_max = 1000000.0;
    cfg.order_size = 100.0;
    cfg.vol_window = 5;

    AvellanedaStoikov strat(cfg);
    OrderManager om;
    Metrics metrics;

    // Feed several snapshots to build vol estimate
    for (int i = 0; i < 10; ++i)
    {
        auto snap = make_snap(1.0 + i * 0.001, 1.1 + i * 0.001,
                              100.0, 100.0, 1000000000ULL + i * 1000000000ULL);
        strat.on_book(snap, om, metrics);
    }

    // After warm-up, bid < ask
    if (om.bid_order && om.ask_order)
    {
        REQUIRE(om.bid_order->price < om.ask_order->price);
        REQUIRE(om.bid_order->price > 0.0);
        REQUIRE(om.ask_order->price > 0.0);
    }
}

TEST_CASE("AvellanedaStoikov: bid fill updates inventory", "[strategy]")
{
    ASConfig cfg;
    cfg.gamma = 0.01;
    cfg.kappa = 1.5;
    cfg.T = 1.0;
    cfg.q_max = 1000000.0;
    cfg.order_size = 100.0;
    cfg.vol_window = 5;

    AvellanedaStoikov strat(cfg);
    OrderManager om;
    Metrics metrics;

    // Warm up
    for (int i = 0; i < 10; ++i)
    {
        auto snap = make_snap(1.0, 1.1, 100.0, 100.0,
                              1000000000ULL + i * 1000000000ULL);
        strat.on_book(snap, om, metrics);
    }

    int fills_before = metrics.num_fills;

    // Simulate a sell trade that crosses our bid
    if (om.bid_order && om.bid_order->active)
    {
        double bid = om.bid_order->price;
        Trade t = make_trade(true, bid - 0.001, 50.0, 20000000000ULL);
        strat.on_trade(t, om, metrics);

        if (metrics.num_fills > fills_before)
        {
            REQUIRE(metrics.inventory > 0.0);
            REQUIRE(metrics.turnover > 0.0);
        }
    }
}

TEST_CASE("MicropriceAS: microprice between bid and ask", "[strategy]")
{
    // Equal volumes → microprice = mid
    auto snap_equal = make_snap(1.0, 1.1, 100.0, 100.0);
    double mp_equal = compute_microprice(snap_equal);
    REQUIRE(mp_equal == Approx(1.05).epsilon(0.01));

    // More bid volume → microprice closer to ask
    auto snap_bid = make_snap(1.0, 1.1, 900.0, 100.0);
    double mp_bid = compute_microprice(snap_bid);
    REQUIRE(mp_bid > 1.05);
    REQUIRE(mp_bid < 1.1);

    // More ask volume → microprice closer to bid
    auto snap_ask = make_snap(1.0, 1.1, 100.0, 900.0);
    double mp_ask = compute_microprice(snap_ask);
    REQUIRE(mp_ask < 1.05);
    REQUIRE(mp_ask > 1.0);
}

TEST_CASE("MicropriceAS: places valid quotes", "[strategy]")
{
    ASConfig cfg;
    cfg.gamma = 0.01;
    cfg.kappa = 1.5;
    cfg.T = 1.0;
    cfg.q_max = 1000000.0;
    cfg.order_size = 100.0;
    cfg.vol_window = 5;

    MicropriceAS strat(cfg);
    OrderManager om;
    Metrics metrics;

    for (int i = 0; i < 10; ++i)
    {
        auto snap = make_snap(1.0 + i * 0.001, 1.1 + i * 0.001,
                              200.0, 100.0,
                              1000000000ULL + i * 1000000000ULL);
        strat.on_book(snap, om, metrics);
    }

    if (om.bid_order && om.ask_order)
    {
        REQUIRE(om.bid_order->price < om.ask_order->price);
        REQUIRE(om.bid_order->amount == Approx(100.0));
        REQUIRE(om.ask_order->amount == Approx(100.0));
    }
}
