// Tests for ThreadMarketDataSource.

#include "parser/ThreadMarketDataSource.hpp"
#include "parser/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace cmf;

namespace {

// Minimal duck-typed source backed by an in-memory vector.
class VectorSource {
 public:
  explicit VectorSource(std::vector<MarketDataEvent> events)
      : events_(std::move(events)) {}

  bool next(MarketDataEvent& out) {
    if (idx_ >= events_.size()) return false;
    out = events_[idx_++];
    return true;
  }

 private:
  std::vector<MarketDataEvent> events_;
  std::size_t                  idx_{0};
};

MarketDataEvent makeEv(NanoTime ts, uint32_t inst, uint64_t oid) {
  MarketDataEvent ev;
  ev.ts_recv = ts;
  ev.ts_event = ts;
  ev.instrument_id = inst;
  ev.order_id = oid;
  ev.action = Action::Add;
  ev.side = MdSide::Bid;
  ev.size = 1;
  return ev;
}

}  // namespace

TEST_CASE("ThreadMarketDataSource: empty upstream", "[ThreadMarketDataSource]") {
  VectorSource src{{}};
  ThreadMarketDataSource<VectorSource> tmds(src, 8);
  MarketDataEvent ev;
  REQUIRE_FALSE(tmds.next(ev));  // EOF immediately
}

TEST_CASE("ThreadMarketDataSource: relays all events in order",
          "[ThreadMarketDataSource]") {
  std::vector<MarketDataEvent> in;
  for (uint32_t i = 0; i < 100; ++i) in.push_back(makeEv(1000 + i, 42, i));
  VectorSource src{std::move(in)};

  ThreadMarketDataSource<VectorSource> tmds(src, 8);

  std::vector<MarketDataEvent> out;
  MarketDataEvent ev;
  while (tmds.next(ev)) out.push_back(ev);

  REQUIRE(out.size() == 100);
  for (uint32_t i = 0; i < 100; ++i) {
    REQUIRE(out[i].ts_recv  == static_cast<NanoTime>(1000 + i));
    REQUIRE(out[i].order_id == i);
  }
}

TEST_CASE("ThreadMarketDataSource: backpressure does not lose events",
          "[ThreadMarketDataSource]") {
  // Many events through a small buffer — exercises the producer-block path.
  constexpr uint32_t kN = 10'000;
  std::vector<MarketDataEvent> in;
  in.reserve(kN);
  for (uint32_t i = 0; i < kN; ++i) in.push_back(makeEv(i, 7, i));
  VectorSource src{std::move(in)};

  ThreadMarketDataSource<VectorSource> tmds(src, 16);  // tiny ring

  uint32_t count = 0;
  MarketDataEvent ev;
  while (tmds.next(ev)) {
    REQUIRE(ev.order_id == count);  // FIFO across many wrap-arounds
    ++count;
  }
  REQUIRE(count == kN);
}

TEST_CASE("ThreadMarketDataSource: destructor with consumer abandoning early "
          "doesn't deadlock", "[ThreadMarketDataSource]") {
  // Producer would block on a small ring; destructor must wake it.
  std::vector<MarketDataEvent> in;
  for (uint32_t i = 0; i < 1000; ++i) in.push_back(makeEv(i, 1, i));
  VectorSource src{std::move(in)};

  {
    ThreadMarketDataSource<VectorSource> tmds(src, 8);
    MarketDataEvent ev;
    // Consume only a handful, then let the destructor run.
    for (int i = 0; i < 4; ++i) REQUIRE(tmds.next(ev));
  }
  // If we got here without hanging, the destructor cleanly stopped the
  // producer thread.
  SUCCEED();
}
