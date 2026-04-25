// Unit tests for FlatMerger and HierarchyMerger.
//
// Both mergers must produce a strictly non-decreasing stream by ts_recv over
// arbitrary numbers of sources of arbitrary lengths. Tested against the same
// fixtures to guarantee equivalent output.

#include "market_data/Merger.hpp"
#include "market_data/EventSource.hpp"

#include "catch2/catch_all.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

using namespace cmf;

namespace
{

MarketDataEvent makeEvent(std::int64_t ts_recv, std::uint32_t sequence = 0,
                          std::uint64_t instrument_id = 0)
{
    MarketDataEvent e;
    e.ts_recv = ts_recv;
    e.sequence = sequence;
    e.instrument_id = instrument_id;
    return e;
}

std::vector<std::unique_ptr<VectorEventSource>>
makeSources(std::vector<std::vector<MarketDataEvent>> streams)
{
    std::vector<std::unique_ptr<VectorEventSource>> srcs;
    srcs.reserve(streams.size());
    for (auto& s : streams)
        srcs.push_back(std::make_unique<VectorEventSource>(std::move(s)));
    return srcs;
}

std::vector<IEventSource*>
asPointers(std::vector<std::unique_ptr<VectorEventSource>>& owned)
{
    std::vector<IEventSource*> out;
    out.reserve(owned.size());
    for (auto& u : owned)
        out.push_back(u.get());
    return out;
}

std::vector<std::int64_t> drainTs(IMerger& m)
{
    std::vector<std::int64_t> out;
    MarketDataEvent e;
    while (m.next(e))
        out.push_back(e.ts_recv);
    return out;
}

std::vector<std::vector<MarketDataEvent>>
makeSortedStreams(std::size_t n_sources, std::size_t per_source,
                  std::uint32_t seed)
{
    std::mt19937 rng{seed};
    std::uniform_int_distribution<std::int64_t> dist(0, 1'000'000);
    std::vector<std::vector<MarketDataEvent>> out(n_sources);
    for (std::size_t i = 0; i < n_sources; ++i)
    {
        std::vector<std::int64_t> ts(per_source);
        for (auto& t : ts)
            t = dist(rng);
        std::sort(ts.begin(), ts.end());
        out[i].reserve(per_source);
        std::uint32_t seq = 0;
        for (auto t : ts)
            out[i].push_back(makeEvent(t, seq++, i));
    }
    return out;
}

std::vector<std::int64_t>
expectedMergedTs(const std::vector<std::vector<MarketDataEvent>>& streams)
{
    std::vector<std::int64_t> all;
    for (const auto& s : streams)
        for (const auto& e : s)
            all.push_back(e.ts_recv);
    std::sort(all.begin(), all.end());
    return all;
}

} // namespace

// ----- FlatMerger --------------------------------------------------------

TEST_CASE("FlatMerger - zero sources returns false", "[Merger][Flat]")
{
    std::vector<IEventSource*> empty;
    FlatMerger m(empty);
    MarketDataEvent e;
    REQUIRE_FALSE(m.next(e));
}

TEST_CASE("FlatMerger - single source acts as pass-through", "[Merger][Flat]")
{
    auto owned = makeSources({{makeEvent(10), makeEvent(20), makeEvent(30)}});
    auto srcs = asPointers(owned);
    FlatMerger m(srcs);
    REQUIRE(drainTs(m) == std::vector<std::int64_t>{10, 20, 30});
}

TEST_CASE("FlatMerger - 3 sources interleaved", "[Merger][Flat]")
{
    auto owned = makeSources({
        {makeEvent(1), makeEvent(4), makeEvent(7)},
        {makeEvent(2), makeEvent(5), makeEvent(8)},
        {makeEvent(3), makeEvent(6), makeEvent(9)},
    });
    auto srcs = asPointers(owned);
    FlatMerger m(srcs);
    REQUIRE(drainTs(m) == std::vector<std::int64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9});
}

TEST_CASE("FlatMerger - unequal-length sources", "[Merger][Flat]")
{
    auto owned = makeSources({
        {makeEvent(1)},
        {makeEvent(2), makeEvent(3), makeEvent(4), makeEvent(5)},
        {}, // empty source is legal
        {makeEvent(6)},
    });
    auto srcs = asPointers(owned);
    FlatMerger m(srcs);
    REQUIRE(drainTs(m) == std::vector<std::int64_t>{1, 2, 3, 4, 5, 6});
}

TEST_CASE("FlatMerger - random stress 20 sources x 500 events",
          "[Merger][Flat]")
{
    auto streams = makeSortedStreams(20, 500, /*seed=*/42);
    auto expected = expectedMergedTs(streams);

    auto owned = makeSources(std::move(streams));
    auto srcs = asPointers(owned);
    FlatMerger m(srcs);
    REQUIRE(drainTs(m) == expected);
}

// ----- HierarchyMerger ---------------------------------------------------

TEST_CASE("HierarchyMerger - zero sources returns false",
          "[Merger][Hierarchy]")
{
    std::vector<IEventSource*> empty;
    HierarchyMerger m(empty);
    MarketDataEvent e;
    REQUIRE_FALSE(m.next(e));
}

TEST_CASE("HierarchyMerger - single source acts as pass-through",
          "[Merger][Hierarchy]")
{
    auto owned = makeSources({{makeEvent(10), makeEvent(20), makeEvent(30)}});
    auto srcs = asPointers(owned);
    HierarchyMerger m(srcs);
    REQUIRE(drainTs(m) == std::vector<std::int64_t>{10, 20, 30});
}

TEST_CASE("HierarchyMerger - N=7 (odd tree) preserves order",
          "[Merger][Hierarchy]")
{
    auto streams = makeSortedStreams(7, 100, /*seed=*/7);
    auto expected = expectedMergedTs(streams);
    auto owned = makeSources(std::move(streams));
    auto srcs = asPointers(owned);
    HierarchyMerger m(srcs);
    REQUIRE(drainTs(m) == expected);
}

TEST_CASE("HierarchyMerger - random stress 20 sources x 500 events",
          "[Merger][Hierarchy]")
{
    auto streams = makeSortedStreams(20, 500, /*seed=*/42);
    auto expected = expectedMergedTs(streams);
    auto owned = makeSources(std::move(streams));
    auto srcs = asPointers(owned);
    HierarchyMerger m(srcs);
    REQUIRE(drainTs(m) == expected);
}

// ----- equivalence of both strategies -------------------------------------

TEST_CASE("Flat and Hierarchy mergers produce identical sequences",
          "[Merger][Equivalence]")
{
    for (std::uint32_t seed : {1u, 2u, 17u, 999u})
    {
        auto streams = makeSortedStreams(16, 250, seed);

        auto ownedA = makeSources(streams);
        auto srcsA = asPointers(ownedA);
        FlatMerger flat(srcsA);
        auto flat_out = drainTs(flat);

        auto ownedB = makeSources(streams);
        auto srcsB = asPointers(ownedB);
        HierarchyMerger hier(srcsB);
        auto hier_out = drainTs(hier);

        REQUIRE(flat_out == hier_out);
    }
}
