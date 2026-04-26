#include "common/MarketDataEvent.hpp"
#include "dispatch/Dispatcher.hpp"
#include "dispatch/EventSink.hpp"
#include "dispatch/LobRegistry.hpp"
#include "dispatch/ShardedDispatcher.hpp"
#include "lob/LimitOrderBook.hpp"
#include "lob/OrderIndex.hpp"

#include "catch2/catch_all.hpp"

#include <random>
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

MarketDataEvent mk(NanoTime ts, char a, uint32_t inst, uint64_t oid, char side, double px, uint32_t sz) {
    MarketDataEvent e{};
    e.ts_recv = ts; e.action = a; e.instrument_id = inst; e.order_id = oid;
    e.side = side; e.price = px; e.size = sz;
    return e;
}

std::vector<MarketDataEvent> make_synthetic(std::size_t n, std::size_t n_inst, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<uint32_t> inst_dist(1, static_cast<uint32_t>(n_inst));
    std::uniform_real_distribution<double>  px_off(-0.5, 0.5);
    std::uniform_int_distribution<uint32_t> sz_dist(1, 20);
    std::uniform_int_distribution<int>      side_dist(0, 1);
    std::uniform_int_distribution<int>      action_dist(0, 99);

    std::vector<MarketDataEvent> events;
    events.reserve(n);
    std::vector<uint64_t> live;
    live.reserve(n);
    uint64_t next_oid = 1;
    NanoTime t = 1;
    for (std::size_t i = 0; i < n; ++i) {
        const auto inst = inst_dist(rng);
        const char side = side_dist(rng) ? 'B' : 'A';
        const double px = 100.0 + inst + px_off(rng);
        const uint32_t sz = sz_dist(rng);
        const int kind = action_dist(rng);
        if (live.empty() || kind < 60) {
            const uint64_t oid = next_oid++;
            live.push_back(oid);
            events.push_back(mk(t++, 'A', inst, oid, side, px, sz));
        } else if (kind < 80) {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            const std::size_t k = pick(rng);
            events.push_back(mk(t++, 'C', 0, live[k], 'N', 0.0, 0));
            live[k] = live.back(); live.pop_back();
        } else {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            events.push_back(mk(t++, 'F', 0, live[pick(rng)], 'N', 0.0, 1));
        }
    }
    return events;
}

void final_bbo_for_inst(const LobRegistry& reg, uint32_t inst, double& bp, uint64_t& bq, double& ap, uint64_t& aq) {
    bp = ap = 0.0; bq = aq = 0;
    auto* b = reg.try_get(inst);
    if (!b) return;
    LimitOrderBook::AggQty q;
    double p;
    if (b->best_bid(p, q)) { bp = p; bq = q; }
    if (b->best_ask(p, q)) { ap = p; aq = q; }
}

void final_bbo_sharded(const ShardedDispatcher<StubMerger>& sd, uint32_t inst,
                       double& bp, uint64_t& bq, double& ap, uint64_t& aq) {
    bp = ap = 0.0; bq = aq = 0;
    for (std::size_t i = 0; i < sd.shards(); ++i) {
        if (auto* b = sd.registry(i).try_get(inst)) {
            LimitOrderBook::AggQty q;
            double p;
            if (b->best_bid(p, q)) { bp = p; bq = q; }
            if (b->best_ask(p, q)) { ap = p; aq = q; }
            return;
        }
    }
}

} // namespace

TEST_CASE("ShardedDispatcher - matches sequential per-instrument BBO", "[sharded]") {
    constexpr std::size_t N = 5000;
    constexpr std::size_t N_INST = 7;
    auto events = make_synthetic(N, N_INST, 1234);

    StubMerger m_seq{events, 0};
    LobRegistry reg_seq;
    OrderIndex  idx_seq;
    NullSink    sink;
    Dispatcher disp(m_seq, reg_seq, idx_seq, sink);
    disp.run();

    for (std::size_t shards : {2u, 4u}) {
        StubMerger m_sh{events, 0};
        OrderIndex idx_sh;
        ShardedDispatcher<StubMerger> sd(m_sh, idx_sh, shards);
        sd.run();
        REQUIRE(sd.events_processed() == disp.events_processed());

        for (uint32_t inst = 1; inst <= N_INST; ++inst) {
            double bp_s, ap_s, bp_h, ap_h;
            uint64_t bq_s, aq_s, bq_h, aq_h;
            final_bbo_for_inst(reg_seq, inst, bp_s, bq_s, ap_s, aq_s);
            final_bbo_sharded(sd, inst, bp_h, bq_h, ap_h, aq_h);
            INFO("inst=" << inst << " shards=" << shards);
            REQUIRE(bp_s == Catch::Approx(bp_h));
            REQUIRE(bq_s == bq_h);
            REQUIRE(ap_s == Catch::Approx(ap_h));
            REQUIRE(aq_s == aq_h);
        }
    }
}

TEST_CASE("ShardedDispatcher - workers cover all instruments", "[sharded]") {
    auto events = make_synthetic(2000, 16, 99);
    StubMerger m{events, 0};
    OrderIndex idx;
    ShardedDispatcher<StubMerger> sd(m, idx, 4);
    sd.run();
    REQUIRE(sd.total_books() <= 16u);
    REQUIRE(sd.total_books() > 0u);
}
