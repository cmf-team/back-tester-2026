// EventQueue: thread-safety smoke tests. Single-threaded behaviour is
// covered along the way (push/pop/close).

#include "pipeline/EventQueue.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using cmf::EventQueue;

TEST_CASE("EventQueue: FIFO ordering on a single thread", "[queue]") {
  EventQueue<int> q(8);
  for (int i = 0; i < 5; ++i)
    REQUIRE(q.push(i));
  for (int i = 0; i < 5; ++i) {
    auto v = q.pop();
    REQUIRE(v.has_value());
    REQUIRE(*v == i);
  }
}

TEST_CASE("EventQueue: close drains pending then returns nullopt", "[queue]") {
  EventQueue<int> q(8);
  q.push(1);
  q.push(2);
  q.close();
  REQUIRE(*q.pop() == 1);
  REQUIRE(*q.pop() == 2);
  REQUIRE_FALSE(q.pop().has_value());
}

TEST_CASE("EventQueue: producer/consumer threads", "[queue][threads]") {
  EventQueue<int> q(64);
  std::atomic<int> sum{0};

  std::thread consumer([&] {
    while (true) {
      auto v = q.pop();
      if (!v) break;
      sum += *v;
    }
  });
  std::thread producer([&] {
    for (int i = 1; i <= 1000; ++i)
      q.push(i);
    q.close();
  });
  producer.join();
  consumer.join();

  REQUIRE(sum.load() == 1000 * 1001 / 2);
}

TEST_CASE("EventQueue: bounded capacity blocks producer", "[queue][threads]") {
  EventQueue<int> q(2);
  REQUIRE(q.push(1));
  REQUIRE(q.push(2));

  std::atomic<bool> pushed3{false};
  std::thread t([&] {
    q.push(3);
    pushed3 = true;
  });
  // Give the producer time to block.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  REQUIRE_FALSE(pushed3.load());

  // Make room.
  REQUIRE(*q.pop() == 1);
  t.join();
  REQUIRE(pushed3.load());
}
