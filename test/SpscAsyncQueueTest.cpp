// Tests for SpscAsyncQueue. Mirrors AsyncQueueTest to confirm the lock-free
// implementation has identical externally visible semantics.

#include "common/SpscAsyncQueue.hpp"

#include "catch2/catch_all.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace cmf;

TEST_CASE("SpscAsyncQueue rounds capacity up to power of 2",
          "[SpscAsyncQueue]") {
  SpscAsyncQueue<int> q1(8);   REQUIRE(q1.capacity() == 8);
  SpscAsyncQueue<int> q2(7);   REQUIRE(q2.capacity() == 8);
  SpscAsyncQueue<int> q3(1);   REQUIRE(q3.capacity() == 2);  // floor of 2
  SpscAsyncQueue<int> q4(33);  REQUIRE(q4.capacity() == 64);
}

TEST_CASE("SpscAsyncQueue single push/pop preserves value",
          "[SpscAsyncQueue]") {
  SpscAsyncQueue<int> q(4);
  REQUIRE(q.push(42));
  int v = 0;
  REQUIRE(q.pop(v));
  REQUIRE(v == 42);
}

TEST_CASE("SpscAsyncQueue close on empty returns false from pop",
          "[SpscAsyncQueue]") {
  SpscAsyncQueue<int> q(4);
  q.close();
  int v = 0;
  REQUIRE_FALSE(q.pop(v));
}

TEST_CASE("SpscAsyncQueue close drains remaining before returning false",
          "[SpscAsyncQueue]") {
  SpscAsyncQueue<int> q(4);
  REQUIRE(q.push(1));
  REQUIRE(q.push(2));
  q.close();

  int v = 0;
  REQUIRE(q.pop(v));  REQUIRE(v == 1);
  REQUIRE(q.pop(v));  REQUIRE(v == 2);
  REQUIRE_FALSE(q.pop(v));
}

TEST_CASE("SpscAsyncQueue order is FIFO across wrap-around",
          "[SpscAsyncQueue]") {
  SpscAsyncQueue<int> q(4);
  for (int round = 0; round < 3; ++round) {
    for (int i = 0; i < 4; ++i) REQUIRE(q.push(round * 10 + i));
    for (int i = 0; i < 4; ++i) {
      int v = 0;
      REQUIRE(q.pop(v));
      REQUIRE(v == round * 10 + i);
    }
  }
}

TEST_CASE("SpscAsyncQueue producer blocks when full, wakes at low water",
          "[SpscAsyncQueue]") {
  constexpr std::size_t kCap = 8;
  SpscAsyncQueue<int> q(kCap);

  std::atomic<int> pushed{0};
  std::thread producer([&] {
    for (int i = 0; i < 12; ++i) {
      REQUIRE(q.push(i));
      pushed.store(i + 1, std::memory_order_release);
    }
  });

  // Producer should fill the ring then park.
  for (int spin = 0; spin < 1000 && pushed.load() < static_cast<int>(kCap); ++spin) {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  REQUIRE(pushed.load() == static_cast<int>(kCap));

  // Pop three — size goes 8 -> 7 -> 6 -> 5; producer must stay parked.
  for (int i = 0; i < 3; ++i) {
    int v = 0;
    REQUIRE(q.pop(v));
    REQUIRE(v == i);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  REQUIRE(pushed.load() == static_cast<int>(kCap));

  // Pop the 4th — size hits low_water=4, producer unblocks.
  int v = 0;
  REQUIRE(q.pop(v));  REQUIRE(v == 3);

  for (int i = 4; i < 12; ++i) {
    REQUIRE(q.pop(v));
    REQUIRE(v == i);
  }
  producer.join();
  REQUIRE(pushed.load() == 12);
}

TEST_CASE("SpscAsyncQueue requestStop unblocks waiting producer",
          "[SpscAsyncQueue]") {
  SpscAsyncQueue<int> q(2);
  REQUIRE(q.push(1));
  REQUIRE(q.push(2));  // ring full

  std::atomic<bool> producer_returned{false};
  std::atomic<bool> producer_succeeded{false};
  std::thread producer([&] {
    const bool ok = q.push(3);
    producer_succeeded.store(ok, std::memory_order_release);
    producer_returned.store(true, std::memory_order_release);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  REQUIRE_FALSE(producer_returned.load());

  q.requestStop();
  producer.join();
  REQUIRE(producer_returned.load());
  REQUIRE_FALSE(producer_succeeded.load());
}

TEST_CASE("SpscAsyncQueue requestStop unblocks waiting consumer",
          "[SpscAsyncQueue]") {
  SpscAsyncQueue<int> q(4);
  std::atomic<bool> consumer_returned{false};
  std::atomic<bool> consumer_got_value{false};
  std::thread consumer([&] {
    int v = 0;
    const bool ok = q.pop(v);
    consumer_got_value.store(ok, std::memory_order_release);
    consumer_returned.store(true, std::memory_order_release);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  REQUIRE_FALSE(consumer_returned.load());

  q.requestStop();
  consumer.join();
  REQUIRE(consumer_returned.load());
  REQUIRE_FALSE(consumer_got_value.load());
}

TEST_CASE("SpscAsyncQueue throughput: cross-thread FIFO at 10K events",
          "[SpscAsyncQueue]") {
  constexpr int kN = 10'000;
  SpscAsyncQueue<int> q(16);

  std::thread producer([&] {
    for (int i = 0; i < kN; ++i) REQUIRE(q.push(i));
    q.close();
  });

  int expected = 0;
  int v = 0;
  while (q.pop(v)) {
    REQUIRE(v == expected);
    ++expected;
  }
  producer.join();
  REQUIRE(expected == kN);
}
