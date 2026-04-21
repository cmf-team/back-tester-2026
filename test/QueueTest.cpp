#include "common/Queue.hpp"
#include "catch2/catch_all.hpp"

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace cmf;

struct ComplexData {
  int id;
  double value;
  std::string name;
};

TEMPLATE_TEST_CASE("Queue - push and pop single element", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  queue.push(42);

  int result = -1;
  bool popped = queue.pop([&](int &&value) { result = value; });

  REQUIRE(popped == true);
  REQUIRE(result == 42);
}

TEMPLATE_TEST_CASE("Queue - push and pop multiple elements", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  queue.push(1);
  queue.push(2);
  queue.push(3);

  std::vector<int> results;
  (void)queue.pop([&](int &&value) { results.push_back(value); });
  (void)queue.pop([&](int &&value) { results.push_back(value); });
  (void)queue.pop([&](int &&value) { results.push_back(value); });

  REQUIRE(results.size() == 3);
  REQUIRE(results[0] == 1);
  REQUIRE(results[1] == 2);
  REQUIRE(results[2] == 3);
}

TEMPLATE_TEST_CASE("Queue - FIFO order", "[Queue]", BlockingQueue<int>,
                   LockFreeQueue<int>) {
  TestType queue;
  for (int i = 0; i < 10; ++i) {
    queue.push(i);
  }

  for (int i = 0; i < 10; ++i) {
    int result = -1;
    (void)queue.pop([&](int &&value) { result = value; });
    REQUIRE(result == i);
  }
}

TEMPLATE_TEST_CASE("Queue - size tracking", "[Queue]", BlockingQueue<int>,
                   LockFreeQueue<int>) {
  TestType queue;

  REQUIRE(queue.size() == 0);
  queue.push(1);
  REQUIRE(queue.size() == 1);
  queue.push(2);
  REQUIRE(queue.size() == 2);
  queue.push(3);
  REQUIRE(queue.size() == 3);

  (void)queue.pop([&]([[maybe_unused]] int &&value) {});
  REQUIRE(queue.size() == 2);
  (void)queue.pop([&]([[maybe_unused]] int &&value) {});
  REQUIRE(queue.size() == 1);
  (void)queue.pop([&]([[maybe_unused]] int &&value) {});
  REQUIRE(queue.size() == 0);
}

TEMPLATE_TEST_CASE("Queue - is_closed returns false initially", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  REQUIRE(queue.is_closed() == false);
}

TEMPLATE_TEST_CASE("Queue - is_closed returns true after close", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  queue.close();
  REQUIRE(queue.is_closed() == true);
}

TEMPLATE_TEST_CASE("Queue - pop returns false when closed and empty", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  queue.close();

  bool popped = queue.pop([&]([[maybe_unused]] int &&value) {});
  REQUIRE(popped == false);
}

TEMPLATE_TEST_CASE(
    "Queue - pop returns true for remaining elements after close", "[Queue]",
    BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  queue.push(10);
  queue.push(20);
  queue.close();

  bool popped1 = queue.pop([&]([[maybe_unused]] int &&value) {});
  REQUIRE(popped1 == true);

  bool popped2 = queue.pop([&]([[maybe_unused]] int &&value) {});
  REQUIRE(popped2 == true);

  bool popped3 = queue.pop([&]([[maybe_unused]] int &&value) {});
  REQUIRE(popped3 == false);
}

TEMPLATE_TEST_CASE("Queue - push throws when closed", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  queue.close();

  REQUIRE_THROWS_AS(queue.push(42), std::runtime_error);
}

TEST_CASE("BlockingQueue - callback invoked with moved value", "[Queue]") {
  struct MoveDetector {
    int value;
    bool moved = false;

    MoveDetector(int v) : value(v) {}
    MoveDetector(const MoveDetector &) = delete;
    MoveDetector &operator=(const MoveDetector &) = delete;
    MoveDetector(MoveDetector &&other) noexcept
        : value(other.value), moved(true) {}
    MoveDetector &operator=(MoveDetector &&other) noexcept {
      value = other.value;
      moved = true;
      return *this;
    }
  };

  BlockingQueue<MoveDetector> queue;
  queue.push(MoveDetector(99));

  bool was_moved = false;
  (void)queue.pop([&](MoveDetector &&item) {
    was_moved = item.moved;
    REQUIRE(item.value == 99);
  });

  REQUIRE(was_moved == true);
}

TEMPLATE_TEST_CASE("Queue - multiple threads pushing and popping", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  std::vector<int> results;

  std::thread producer([&]() {
    for (int i = 0; i < 100; ++i) {
      queue.push(i);
    }
    queue.close();
  });

  std::thread consumer([&]() {
    while (queue.pop([&](int &&value) { results.push_back(value); })) {
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(results.size() == 100);
  for (int i = 0; i < 100; ++i) {
    REQUIRE(results[i] == i);
  }
}

TEMPLATE_TEST_CASE("Queue - works with complex types", "[Queue]",
                   BlockingQueue<ComplexData>, LockFreeQueue<ComplexData>) {
  TestType queue;
  queue.push(ComplexData{1, 3.14, "test"});
  queue.push(ComplexData{2, 2.71, "data"});

  ComplexData result1;
  (void)queue.pop([&](ComplexData &&d) { result1 = std::move(d); });

  ComplexData result2;
  (void)queue.pop([&](ComplexData &&d) { result2 = std::move(d); });

  REQUIRE(result1.id == 1);
  REQUIRE(result1.value == Catch::Approx(3.14));
  REQUIRE(result1.name == "test");

  REQUIRE(result2.id == 2);
  REQUIRE(result2.value == Catch::Approx(2.71));
  REQUIRE(result2.name == "data");
}

TEST_CASE("BlockingQueue - multiple producers multiple consumers", "[Queue]") {
  BlockingQueue<int> queue;
  std::vector<int> results;
  std::mutex results_mutex;

  auto producer = [&](int start, int end) {
    for (int i = start; i < end; ++i) {
      queue.push(i);
    }
  };

  auto consumer = [&]() {
    while (queue.pop([&](int &&value) {
      std::lock_guard lock(results_mutex);
      results.push_back(value);
    })) {
    }
  };

  std::thread p1(producer, 0, 25);
  std::thread p2(producer, 25, 50);
  std::thread c1(consumer);
  std::thread c2(consumer);

  p1.join();
  p2.join();
  queue.close();
  c1.join();
  c2.join();

  REQUIRE(results.size() == 50);
  std::sort(results.begin(), results.end());
  for (int i = 0; i < 50; ++i) {
    REQUIRE(results[i] == i);
  }
}

TEMPLATE_TEST_CASE("Queue - push_batch single batch", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  int items[] = {10, 20, 30, 40, 50};
  queue.push_batch(items, 5);

  std::vector<int> results;
  for (int i = 0; i < 5; ++i) {
    (void)queue.pop([&](int &&value) { results.push_back(value); });
  }

  REQUIRE(results.size() == 5);
  REQUIRE(results[0] == 10);
  REQUIRE(results[1] == 20);
  REQUIRE(results[2] == 30);
  REQUIRE(results[3] == 40);
  REQUIRE(results[4] == 50);
}

TEMPLATE_TEST_CASE("Queue - push_batch multiple batches", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  int batch1[] = {1, 2, 3};
  int batch2[] = {4, 5, 6};
  int batch3[] = {7, 8, 9};

  queue.push_batch(batch1, 3);
  queue.push_batch(batch2, 3);
  queue.push_batch(batch3, 3);

  std::vector<int> results;
  for (int i = 0; i < 9; ++i) {
    (void)queue.pop([&](int &&value) { results.push_back(value); });
  }

  REQUIRE(results.size() == 9);
  for (int i = 0; i < 9; ++i) {
    REQUIRE(results[i] == i + 1);
  }
}

TEMPLATE_TEST_CASE("Queue - push_batch empty batch", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  int items[] = {1, 2, 3};
  queue.push_batch(items, 0); // Push nothing

  REQUIRE(queue.size() == 0);
  REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue - push_batch throws when closed", "[Queue]") {
  BlockingQueue<int> queue;
  int items[] = {1, 2, 3};
  queue.close();

  REQUIRE_THROWS_AS(queue.push_batch(items, 3), std::runtime_error);
}

TEMPLATE_TEST_CASE("Queue - push_batch mixed with push", "[Queue]",
                   BlockingQueue<int>, LockFreeQueue<int>) {
  TestType queue;
  queue.push(1);
  int batch[] = {2, 3, 4};
  queue.push_batch(batch, 3);
  queue.push(5);

  std::vector<int> results;
  for (int i = 0; i < 5; ++i) {
    (void)queue.pop([&](int &&value) { results.push_back(value); });
  }

  REQUIRE(results.size() == 5);
  REQUIRE(results[0] == 1);
  REQUIRE(results[1] == 2);
  REQUIRE(results[2] == 3);
  REQUIRE(results[3] == 4);
  REQUIRE(results[4] == 5);
}
