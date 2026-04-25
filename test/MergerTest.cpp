// MergerFlat / MergerHierarchy unit tests + behavioural-equivalence test.
// The equivalence test is the most important one: it asserts the two
// merger implementations are interchangeable from the caller's point of
// view.

#include "pipeline/IEventSource.hpp"
#include "pipeline/MergerFlat.hpp"
#include "pipeline/MergerHierarchy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

using cmf::EventSourcePtr;
using cmf::MarketDataEvent;
using cmf::MergerFlat;
using cmf::MergerHierarchy;
using cmf::VectorEventSource;

namespace {

MarketDataEvent ev(cmf::NanoTime ts, std::uint32_t iid = 0,
                   cmf::OrderId oid = 0) {
  MarketDataEvent e;
  e.timestamp     = ts;
  e.instrument_id = iid;
  e.order_id      = oid;
  return e;
}

EventSourcePtr fromTs(std::vector<cmf::NanoTime> tss) {
  std::vector<MarketDataEvent> evs;
  evs.reserve(tss.size());
  for (auto t : tss)
    evs.push_back(ev(t));
  return std::make_unique<VectorEventSource>(std::move(evs));
}

template <class Merger>
std::vector<cmf::NanoTime> drain(Merger &m) {
  std::vector<cmf::NanoTime> out;
  MarketDataEvent e;
  while (m.next(e))
    out.push_back(e.timestamp);
  return out;
}

template <class Merger>
std::vector<cmf::NanoTime> mergeWith(std::vector<EventSourcePtr> srcs) {
  Merger m(std::move(srcs));
  return drain(m);
}

} // namespace

TEST_CASE("Merger flat: simple two-stream merge", "[merger][flat]") {
  std::vector<EventSourcePtr> srcs;
  srcs.push_back(fromTs({1, 3, 5}));
  srcs.push_back(fromTs({2, 4, 6}));
  MergerFlat m(std::move(srcs));
  auto seq = drain(m);
  REQUIRE(seq == std::vector<cmf::NanoTime>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("Merger flat: empty / single-source / one empty source",
          "[merger][flat][edge]") {
  // empty
  {
    MergerFlat m({});
    MarketDataEvent e;
    REQUIRE_FALSE(m.next(e));
  }
  // single
  {
    std::vector<EventSourcePtr> s;
    s.push_back(fromTs({1, 2, 3}));
    MergerFlat m(std::move(s));
    REQUIRE(drain(m) == std::vector<cmf::NanoTime>{1, 2, 3});
  }
  // one of two empty
  {
    std::vector<EventSourcePtr> s;
    s.push_back(fromTs({}));
    s.push_back(fromTs({1, 2}));
    MergerFlat m(std::move(s));
    REQUIRE(drain(m) == std::vector<cmf::NanoTime>{1, 2});
  }
}

TEST_CASE("Merger hierarchy: arbitrary fan-in (k=5, padding to 8)",
          "[merger][hierarchy]") {
  std::vector<EventSourcePtr> srcs;
  srcs.push_back(fromTs({1, 10}));
  srcs.push_back(fromTs({2, 11}));
  srcs.push_back(fromTs({3, 12}));
  srcs.push_back(fromTs({4, 13}));
  srcs.push_back(fromTs({5, 14}));
  MergerHierarchy m(std::move(srcs));
  auto seq = drain(m);
  REQUIRE(seq.size() == 10);
  REQUIRE(std::is_sorted(seq.begin(), seq.end()));
}

TEST_CASE("Merger flat & hierarchy produce the same merged order on random "
          "inputs",
          "[merger][equivalence]") {
  std::mt19937 rng(0xC0FFEEu);
  for (int trial = 0; trial < 25; ++trial) {
    const std::size_t k =
        std::uniform_int_distribution<std::size_t>{1, 10}(rng);
    std::vector<std::vector<cmf::NanoTime>> streams(k);
    for (auto &s : streams) {
      const std::size_t n =
          std::uniform_int_distribution<std::size_t>{0, 50}(rng);
      s.reserve(n);
      cmf::NanoTime t = 0;
      for (std::size_t i = 0; i < n; ++i) {
        t += std::uniform_int_distribution<int>{1, 100}(rng);
        s.push_back(t);
      }
    }
    auto buildSources = [&]() {
      std::vector<EventSourcePtr> ss;
      for (auto &s : streams)
        ss.push_back(fromTs(s));
      return ss;
    };
    auto a = mergeWith<MergerFlat>(buildSources());
    auto b = mergeWith<MergerHierarchy>(buildSources());
    REQUIRE(a == b);

    // Sanity: result is sorted and matches the multiset of inputs.
    std::vector<cmf::NanoTime> all;
    for (auto &s : streams)
      all.insert(all.end(), s.begin(), s.end());
    std::sort(all.begin(), all.end());
    REQUIRE(a == all);
  }
}

TEST_CASE("Merger hierarchy: stable tie-break by leaf id",
          "[merger][hierarchy][stability]") {
  // Same timestamp on two streams: the leaf with smaller id must win.
  std::vector<EventSourcePtr> srcs;
  std::vector<MarketDataEvent> a{ev(5, /*iid=*/1), ev(10, /*iid=*/1)};
  std::vector<MarketDataEvent> b{ev(5, /*iid=*/2), ev(10, /*iid=*/2)};
  srcs.push_back(std::make_unique<VectorEventSource>(std::move(a)));
  srcs.push_back(std::make_unique<VectorEventSource>(std::move(b)));
  MergerHierarchy m(std::move(srcs));

  MarketDataEvent e;
  REQUIRE(m.next(e)); REQUIRE(e.instrument_id == 1);
  REQUIRE(m.next(e)); REQUIRE(e.instrument_id == 2);
  REQUIRE(m.next(e)); REQUIRE(e.instrument_id == 1);
  REQUIRE(m.next(e)); REQUIRE(e.instrument_id == 2);
  REQUIRE_FALSE(m.next(e));
}
