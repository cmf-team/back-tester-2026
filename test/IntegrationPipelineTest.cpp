#include "common/MarketDataEvent.hpp"
#include "dispatch/Dispatcher.hpp"
#include "dispatch/EventSink.hpp"
#include "dispatch/LobRegistry.hpp"
#include "dispatch/ShardedDispatcher.hpp"
#include "ingestion/EventQueue.hpp"
#include "ingestion/FlatMerger.hpp"
#include "ingestion/Producer.hpp"
#include "lob/LimitOrderBook.hpp"
#include "lob/OrderIndex.hpp"

#include "catch2/catch_all.hpp"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace cmf;

namespace {

std::filesystem::path discover_dataset() {
    if (const char* env = std::getenv("CMF_HW1_MBO_FILE"); env && *env) {
        std::filesystem::path p{env};
        if (std::filesystem::is_regular_file(p)) return p;
    }
    const std::filesystem::path candidate =
        std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : "")
        / "development/CMF/HW_1-20260416T234811Z-3-001/HW_1"
          "/XEUR-20260409-HJTR7RCAKT1/XEUR-20260409-HJTR7RCAKT"
          "/xeur-eobi-20260309.mbo.json";
    if (std::filesystem::is_regular_file(candidate)) return candidate;
    return {};
}

} // namespace

TEST_CASE("Integration - pipeline on real Eurex file matches pinned values", "[integration]") {
    const auto path = discover_dataset();
    if (path.empty()) {
        WARN("dataset not found, skipping (set CMF_HW1_MBO_FILE)");
        return;
    }

    EventQueue q;
    Producer   prod(path, q);
    std::vector<EventQueue*> ptrs{&q};
    FlatMerger  merger(ptrs);
    LobRegistry reg;
    OrderIndex  idx;
    NullSink    sink;

    prod.start();
    merger.start();
    Dispatcher disp(merger, reg, idx, sink);
    disp.run();
    prod.join();

    REQUIRE(disp.events_processed() == 1'305'607u);
    REQUIRE(disp.first_ts() == 1'773'042'761'368'148'840LL);
    REQUIRE(disp.last_ts()  == 1'773'079'202'630'298'007LL);
    REQUIRE(reg.size()      == 151u);

    auto* b = reg.try_get(34197);
    REQUIRE(b);
    double bp = 0, ap = 0;
    LimitOrderBook::AggQty bq = 0, aq = 0;
    REQUIRE(b->best_bid(bp, bq));
    REQUIRE(b->best_ask(ap, aq));
    REQUIRE(bp == Catch::Approx(0.00644));
    REQUIRE(bq == 40u);
    REQUIRE(ap == Catch::Approx(0.00586));
    REQUIRE(aq == 20u);
}

TEST_CASE("Integration - sharded matches sequential on real file", "[integration]") {
    const auto path = discover_dataset();
    if (path.empty()) {
        WARN("dataset not found, skipping");
        return;
    }

    auto run_seq = [&](LobRegistry& reg) {
        EventQueue q;
        Producer   prod(path, q);
        std::vector<EventQueue*> ptrs{&q};
        FlatMerger  merger(ptrs);
        OrderIndex  idx;
        NullSink    sink;
        prod.start();
        merger.start();
        Dispatcher disp(merger, reg, idx, sink);
        disp.run();
        prod.join();
    };

    LobRegistry reg_seq;
    run_seq(reg_seq);

    EventQueue q;
    Producer   prod(path, q);
    std::vector<EventQueue*> ptrs{&q};
    FlatMerger  merger(ptrs);
    OrderIndex  idx;

    prod.start();
    merger.start();
    ShardedDispatcher<FlatMerger> sd(merger, idx, 4);
    sd.run();
    prod.join();

    std::vector<uint32_t> sample = {34197, 34201, 34197, 34306, 34309, 34402, 34517};
    for (auto inst : sample) {
        auto* bs = reg_seq.try_get(inst);
        if (!bs) continue;
        const LimitOrderBook* bh = nullptr;
        for (std::size_t i = 0; i < sd.shards(); ++i)
            if (auto* h = sd.registry(i).try_get(inst)) { bh = h; break; }
        REQUIRE(bh);
        double sbp, sap, hbp, hap;
        LimitOrderBook::AggQty sbq, saq, hbq, haq;
        const bool sb = bs->best_bid(sbp, sbq);
        const bool sa = bs->best_ask(sap, saq);
        const bool hb = bh->best_bid(hbp, hbq);
        const bool ha = bh->best_ask(hap, haq);
        REQUIRE(sb == hb);
        REQUIRE(sa == ha);
        if (sb) { REQUIRE(sbp == Catch::Approx(hbp)); REQUIRE(sbq == hbq); }
        if (sa) { REQUIRE(sap == Catch::Approx(hap)); REQUIRE(saq == haq); }
    }
}
