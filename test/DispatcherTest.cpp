// Dispatcher / ShardedDispatcher tests run end-to-end on synthetic event
// streams. We assert event accounting, snapshot triggering and (for the
// sharded variant) per-instrument routing invariants.

#include "lob/InstrumentBookRegistry.hpp"
#include "parser/MarketDataEvent.hpp"
#include "pipeline/Dispatcher.hpp"
#include "pipeline/IEventSource.hpp"
#include "pipeline/ShardedDispatcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <vector>

using cmf::Action;
using cmf::Dispatcher;
using cmf::InstrumentBookRegistry;
using cmf::MarketDataEvent;
using cmf::MdSide;
using cmf::ShardedDispatcher;
using cmf::VectorEventSource;

namespace {

MarketDataEvent makeAdd(cmf::OrderId oid, cmf::InstrumentId iid, MdSide s,
                        double px, double sz, cmf::NanoTime ts) {
  MarketDataEvent e;
  e.timestamp     = ts;
  e.order_id      = oid;
  e.instrument_id = iid;
  e.side          = s;
  e.price         = px;
  e.size          = sz;
  e.action        = Action::Add;
  return e;
}

} // namespace

TEST_CASE("Dispatcher: events routed and counters accumulate",
          "[dispatcher]") {
  std::vector<MarketDataEvent> evs{
      makeAdd(1, 100, MdSide::Bid, 100.0, 10, 1),
      makeAdd(2, 100, MdSide::Ask, 100.5, 5,  2),
      makeAdd(3, 200, MdSide::Bid,  50.0, 7,  3),
  };
  VectorEventSource src(std::move(evs));
  InstrumentBookRegistry reg;
  Dispatcher disp(src, reg);
  auto stats = disp.run();
  REQUIRE(stats.events_in     == 3);
  REQUIRE(stats.events_routed == 3);
  REQUIRE(stats.first_ts == 1);
  REQUIRE(stats.last_ts  == 3);
  REQUIRE(reg.size() == 2);
  REQUIRE(reg.find(100)->bestBidPrice() == 100.0);
  REQUIRE(reg.find(200)->bestBidPrice() ==  50.0);
}

TEST_CASE("Dispatcher: snapshot hook is invoked at the requested cadence",
          "[dispatcher][snapshot]") {
  std::vector<MarketDataEvent> evs;
  for (int i = 0; i < 10; ++i)
    evs.push_back(makeAdd(/*oid=*/i + 1, /*iid=*/1, MdSide::Bid,
                          100.0 + i * 0.1, 1, i + 1));
  VectorEventSource src(std::move(evs));
  InstrumentBookRegistry reg;

  std::vector<std::uint64_t> snap_seqs;
  Dispatcher disp(src, reg, /*snapshot_every=*/3,
                  [&](std::uint64_t seq, cmf::NanoTime) {
                    snap_seqs.push_back(seq);
                  });
  disp.run();
  REQUIRE(snap_seqs == std::vector<std::uint64_t>{3, 6, 9});
}

TEST_CASE("ShardedDispatcher: routes by instrument_id and preserves "
          "per-instrument event count",
          "[dispatcher][sharded]") {
  // Build a stream covering 4 instruments interleaved.
  std::vector<MarketDataEvent> evs;
  cmf::NanoTime t = 1;
  for (int round = 0; round < 25; ++round) {
    for (cmf::InstrumentId iid : {10u, 20u, 30u, 40u}) {
      evs.push_back(
          makeAdd(/*oid=*/static_cast<cmf::OrderId>(t), iid, MdSide::Bid,
                  100.0 + iid * 0.01, 1, t));
      ++t;
    }
  }
  const auto total = evs.size();
  VectorEventSource src(std::move(evs));

  ShardedDispatcher sd(src, /*workers=*/3);
  auto stats = sd.run();
  REQUIRE(stats.events_in == total);
  REQUIRE(stats.events_routed == total);

  // Sum of per-worker open orders must match how many we added.
  std::uint64_t orders = 0;
  for (std::size_t i = 0; i < sd.numWorkers(); ++i)
    sd.registry(i).forEach([&](auto, const cmf::LimitOrderBook &b) {
      orders += b.openOrders();
    });
  REQUIRE(orders == total);

  // Each instrument must live on exactly one worker.
  for (cmf::InstrumentId iid : {10u, 20u, 30u, 40u}) {
    int hits = 0;
    for (std::size_t i = 0; i < sd.numWorkers(); ++i)
      if (sd.registry(i).find(iid))
        ++hits;
    REQUIRE(hits == 1);
  }
}

TEST_CASE("ShardedDispatcher: snapshot hook fires per worker",
          "[dispatcher][sharded][snapshot]") {
  std::vector<MarketDataEvent> evs;
  cmf::NanoTime t = 1;
  for (int i = 0; i < 60; ++i) {
    evs.push_back(makeAdd(t, /*iid=*/(i % 4) + 1, MdSide::Bid, 1.0, 1, t));
    ++t;
  }
  VectorEventSource src(std::move(evs));

  std::atomic<int> calls{0};
  ShardedDispatcher sd(
      src, /*workers=*/4, /*snapshot_every=*/20,
      [&](std::size_t, const InstrumentBookRegistry &, std::uint64_t,
          cmf::NanoTime) { ++calls; });
  sd.run();

  // 60 events / 20 = 3 snapshot rounds, broadcast to 4 workers.
  REQUIRE(calls.load() == 3 * 4);
}
