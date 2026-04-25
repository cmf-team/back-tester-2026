// Unit tests for SpscRingQueue including a 1M thread-race stress test.

#include "market_data/SpscRingQueue.hpp"

#include "catch2/catch_all.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace cmf;

TEST_CASE("SpscRingQueue - capacity rounds up to next power of two",
          "[SpscRingQueue]")
{
    REQUIRE(SpscRingQueue<int>(1).capacity() == 2);
    REQUIRE(SpscRingQueue<int>(2).capacity() == 2);
    REQUIRE(SpscRingQueue<int>(3).capacity() == 4);
    REQUIRE(SpscRingQueue<int>(5).capacity() == 8);
    REQUIRE(SpscRingQueue<int>(1000).capacity() == 1024);
}

TEST_CASE("SpscRingQueue - rejects zero capacity", "[SpscRingQueue]")
{
    REQUIRE_THROWS_AS(SpscRingQueue<int>(0), std::invalid_argument);
}

TEST_CASE("SpscRingQueue - single-threaded push/pop in order",
          "[SpscRingQueue]")
{
    SpscRingQueue<int> q(8);
    for (int i = 0; i < 8; ++i)
        REQUIRE(q.push(i));

    REQUIRE_FALSE(q.push(99)); // full

    int out = -1;
    for (int i = 0; i < 8; ++i)
    {
        REQUIRE(q.pop(out));
        REQUIRE(out == i);
    }
    REQUIRE_FALSE(q.pop(out));
    REQUIRE(q.empty());
}

TEST_CASE("SpscRingQueue - wraparound after partial drain", "[SpscRingQueue]")
{
    SpscRingQueue<int> q(4);
    REQUIRE(q.push(1));
    REQUIRE(q.push(2));
    REQUIRE(q.push(3));

    int out = 0;
    REQUIRE(q.pop(out));
    REQUIRE(out == 1);
    REQUIRE(q.pop(out));
    REQUIRE(out == 2);

    // Now indices 0..1 freed; push should wrap.
    REQUIRE(q.push(4));
    REQUIRE(q.push(5));
    REQUIRE(q.push(6));
    REQUIRE_FALSE(q.push(7)); // full again

    int expected = 3;
    while (q.pop(out))
        REQUIRE(out == expected++);
    REQUIRE(expected == 7);
}

TEST_CASE("SpscRingQueue - close/done semantics", "[SpscRingQueue]")
{
    SpscRingQueue<int> q(4);
    REQUIRE_FALSE(q.isClosed());
    REQUIRE_FALSE(q.done());

    REQUIRE(q.push(1));
    q.close();
    REQUIRE(q.isClosed());
    REQUIRE_FALSE(q.done()); // still has 1 item

    int out = 0;
    REQUIRE(q.pop(out));
    REQUIRE(out == 1);
    REQUIRE(q.done());
}

TEST_CASE("SpscRingQueue - 1M element producer/consumer race preserves order",
          "[SpscRingQueue][thread]")
{
    constexpr std::uint64_t kN = 1'000'000;
    SpscRingQueue<std::uint64_t> q(1024);

    std::atomic<std::uint64_t> sum_seen{0};
    std::atomic<bool> order_ok{true};

    std::thread consumer([&]
                         {
    std::uint64_t expected = 0;
    std::uint64_t got = 0;
    while (expected < kN) {
      if (q.pop(got)) {
        if (got != expected) {
          order_ok.store(false, std::memory_order_relaxed);
          break;
        }
        sum_seen.fetch_add(got, std::memory_order_relaxed);
        ++expected;
      } else {
        std::this_thread::yield();
      }
    } });

    for (std::uint64_t i = 0; i < kN; ++i)
    {
        while (!q.push(i))
            std::this_thread::yield();
    }
    q.close();

    consumer.join();

    REQUIRE(order_ok.load());
    // 0 + 1 + ... + (kN-1)
    const std::uint64_t expected_sum = kN * (kN - 1) / 2;
    REQUIRE(sum_seen.load() == expected_sum);
    REQUIRE(q.done());
}
