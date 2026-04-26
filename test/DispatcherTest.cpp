#include "common/MarketDataEvent.hpp"
#include "dispatch/Dispatcher.hpp"
#include "dispatch/EventSink.hpp"
#include "dispatch/LobRegistry.hpp"
#include "lob/LimitOrderBook.hpp"
#include "lob/OrderIndex.hpp"

#include "catch2/catch_all.hpp"

#include <vector>

using namespace cmf;

namespace {

struct StubMerger {
    std::vector<MarketDataEvent> events;
    std::size_t i = 0;
    bool next(MarketDataEvent& out) {
        if (i >= events.size()) return false;
        out = events[i++];
        return true;
    }
};

MarketDataEvent mk(NanoTime ts, char action, uint32_t inst, uint64_t order_id,
                   char side, double price, uint32_t size) {
    MarketDataEvent e{};
    e.ts_recv       = ts;
    e.action        = action;
    e.instrument_id = inst;
    e.order_id      = order_id;
    e.side          = side;
    e.price         = price;
    e.size          = size;
    return e;
}

} // namespace

TEST_CASE("Dispatcher - routes Add to correct LOB", "[dispatcher]") {
    StubMerger m{{
        mk(1, 'A', 10, 1, 'B', 100.0, 5),
        mk(2, 'A', 20, 2, 'A', 200.0, 3),
    }};
    LobRegistry reg;
    OrderIndex  idx;
    NullSink    sink;
    Dispatcher disp(m, reg, idx, sink);
    disp.run();

    REQUIRE(reg.size() == 2u);
    auto* b10 = reg.try_get(10);
    auto* b20 = reg.try_get(20);
    REQUIRE(b10);
    REQUIRE(b20);
    double px; LimitOrderBook::AggQty q;
    REQUIRE(b10->best_bid(px, q));
    REQUIRE(px == Catch::Approx(100.0));
    REQUIRE(q == 5u);
    REQUIRE(b20->best_ask(px, q));
    REQUIRE(px == Catch::Approx(200.0));
    REQUIRE(q == 3u);
}

TEST_CASE("Dispatcher - orphan Cancel resolves via OrderIndex", "[dispatcher]") {
    StubMerger m{{
        mk(1, 'A', 10, 1, 'B', 100.0, 5),
        mk(2, 'C', 0,  1, 'N', 0.0,   0),  // no instrument_id, no price, no size
    }};
    LobRegistry reg;
    OrderIndex  idx;
    NullSink    sink;
    Dispatcher disp(m, reg, idx, sink);
    disp.run();

    auto* b = reg.try_get(10);
    REQUIRE(b);
    REQUIRE(b->empty());
    REQUIRE(idx.size() == 0u);
    REQUIRE(disp.orphans_skipped() == 0u);
}

TEST_CASE("Dispatcher - orphan Fill decrements correctly", "[dispatcher]") {
    StubMerger m{{
        mk(1, 'A', 20, 2, 'A', 200.0, 3),
        mk(2, 'F', 0,  2, 'N', 0.0,   1),
    }};
    LobRegistry reg;
    OrderIndex  idx;
    NullSink    sink;
    Dispatcher disp(m, reg, idx, sink);
    disp.run();

    auto* b = reg.try_get(20);
    REQUIRE(b);
    double px; LimitOrderBook::AggQty q;
    REQUIRE(b->best_ask(px, q));
    REQUIRE(px == Catch::Approx(200.0));
    REQUIRE(q == 2u);
    OrderRecord rec;
    REQUIRE(idx.find(2, rec));
    REQUIRE(rec.remaining_qty == 2u);
}

TEST_CASE("Dispatcher - Trade does not mutate LOB", "[dispatcher]") {
    StubMerger m{{
        mk(1, 'A', 30, 5, 'B', 50.0, 4),
        mk(2, 'T', 30, 0, 'B', 50.0, 1),
    }};
    LobRegistry reg;
    OrderIndex  idx;
    NullSink    sink;
    Dispatcher disp(m, reg, idx, sink);
    disp.run();

    auto* b = reg.try_get(30);
    REQUIRE(b);
    double px; LimitOrderBook::AggQty q;
    REQUIRE(b->best_bid(px, q));
    REQUIRE(q == 4u);
    REQUIRE(disp.trades_seen() == 1u);
}

TEST_CASE("Dispatcher - Modify changes price level", "[dispatcher]") {
    StubMerger m{{
        mk(1, 'A', 40, 7, 'B', 99.0, 10),
        mk(2, 'M', 0,  7, 'B', 98.0, 8),
    }};
    LobRegistry reg;
    OrderIndex  idx;
    NullSink    sink;
    Dispatcher disp(m, reg, idx, sink);
    disp.run();

    auto* b = reg.try_get(40);
    REQUIRE(b);
    REQUIRE(b->volume_at('B', LimitOrderBook::scale(99.0)) == 0u);
    REQUIRE(b->volume_at('B', LimitOrderBook::scale(98.0)) == 8u);
}

TEST_CASE("Dispatcher - Reset clears only target instrument", "[dispatcher]") {
    StubMerger m{{
        mk(1, 'A', 50, 1, 'B', 10.0, 1),
        mk(2, 'A', 60, 2, 'B', 20.0, 2),
        mk(3, 'R', 50, 0, 'N', 0.0,  0),
    }};
    LobRegistry reg;
    OrderIndex  idx;
    NullSink    sink;
    Dispatcher disp(m, reg, idx, sink);
    disp.run();

    REQUIRE(reg.try_get(50)->empty());
    REQUIRE_FALSE(reg.try_get(60)->empty());
}
