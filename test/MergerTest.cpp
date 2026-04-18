#include "catch2/catch_all.hpp"
#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include "ingestion/FlatMerger.hpp"
#include "ingestion/HierarchyMerger.hpp"

#include <atomic>
#include <deque>
#include <thread>
#include <vector>

using namespace cmf;

static constexpr int64_t NS = 1'000'000'000LL;

static MarketDataEvent make_event(const auto ts_recv, const uint32_t sequence,
                                  const Action action = Action::Add) {
  MarketDataEvent e;
  e.ts_recv = ts_recv;
  e.sequence = sequence;
  e.action = action;
  return e;
}

TEMPLATE_TEST_CASE("Merger - merges two queues in timestamp order", "[Merger]",
                   FlatMerger<>, HierarchyMerger<>) {
  std::deque<BlockingQueue<MarketDataEvent>> queues(2);
  BlockingQueue<MarketDataEvent> output;

  queues[0].push(make_event(100 * NS, 100));
  queues[0].push(make_event(300 * NS, 300));
  queues[0].close();

  queues[1].push(make_event(50 * NS, 50));
  queues[1].push(make_event(200 * NS, 200));
  queues[1].close();

  std::thread merger_thread([&]() {
    TestType merger(queues, output);
    merger.run();
  });

  std::vector<MarketDataEvent> result;
  while (output.pop([&](MarketDataEvent &&e) { result.push_back(e); })) {
  }

  merger_thread.join();

  REQUIRE(result.size() == 4);
  REQUIRE(result[0].sequence == 50);
  REQUIRE(result[1].sequence == 100);
  REQUIRE(result[2].sequence == 200);
  REQUIRE(result[3].sequence == 300);
  REQUIRE(result[0].ts_recv <= result[1].ts_recv);
  REQUIRE(result[1].ts_recv <= result[2].ts_recv);
  REQUIRE(result[2].ts_recv <= result[3].ts_recv);
  REQUIRE(output.is_closed());
}

TEMPLATE_TEST_CASE("Merger - merges three queues", "[Merger]", FlatMerger<>,
                   HierarchyMerger<>) {
  std::deque<BlockingQueue<MarketDataEvent>> queues(3);
  BlockingQueue<MarketDataEvent> output;

  queues[0].push(make_event(100 * NS, 100));
  queues[0].push(make_event(400 * NS, 400));
  queues[0].close();

  queues[1].push(make_event(200 * NS, 200));
  queues[1].push(make_event(500 * NS, 500));
  queues[1].close();

  queues[2].push(make_event(50 * NS, 50));
  queues[2].push(make_event(300 * NS, 300));
  queues[2].close();

  std::thread merger_thread([&]() {
    TestType merger(queues, output);
    merger.run();
  });

  std::vector<MarketDataEvent> result;
  while (output.pop([&](MarketDataEvent &&e) { result.push_back(e); })) {
  }

  merger_thread.join();

  REQUIRE(result.size() == 6);
  REQUIRE(result[0].sequence == 50);
  REQUIRE(result[1].sequence == 100);
  REQUIRE(result[2].sequence == 200);
  REQUIRE(result[3].sequence == 300);
  REQUIRE(result[4].sequence == 400);
  REQUIRE(result[5].sequence == 500);
}

TEMPLATE_TEST_CASE("Merger - handles single queue", "[Merger]", FlatMerger<>,
                   HierarchyMerger<>) {
  std::deque<BlockingQueue<MarketDataEvent>> queues(1);
  BlockingQueue<MarketDataEvent> output;
  queues[0].push(make_event(100 * NS, 100));
  queues[0].push(make_event(150 * NS, 150));
  queues[0].push(make_event(200 * NS, 200));
  queues[0].close();

  std::thread merger_thread([&]() {
    TestType merger(queues, output);
    merger.run();
  });

  std::vector<MarketDataEvent> result;
  while (output.pop([&](MarketDataEvent &&e) { result.push_back(e); })) {
  }

  merger_thread.join();

  REQUIRE(result.size() == 3);
  REQUIRE(result[0].sequence == 100);
  REQUIRE(result[1].sequence == 150);
  REQUIRE(result[2].sequence == 200);
}

TEMPLATE_TEST_CASE("Merger - handles empty queues", "[Merger]", FlatMerger<>,
                   HierarchyMerger<>) {
  std::deque<BlockingQueue<MarketDataEvent>> queues(2);
  BlockingQueue<MarketDataEvent> output;
  queues[0].push(make_event(100 * NS, 100));
  queues[0].close();

  queues[1].close();

  std::thread merger_thread([&]() {
    TestType merger(queues, output);
    merger.run();
  });

  std::vector<MarketDataEvent> result;
  while (output.pop([&](MarketDataEvent &&e) { result.push_back(e); })) {
  }

  merger_thread.join();

  REQUIRE(result.size() == 1);
  REQUIRE(result[0].sequence == 100);
}

TEMPLATE_TEST_CASE("Merger - all queues empty", "[Merger]", FlatMerger<>,
                   HierarchyMerger<>) {
  std::deque<BlockingQueue<MarketDataEvent>> queues(2);
  BlockingQueue<MarketDataEvent> output;
  queues[0].close();
  queues[1].close();

  std::thread merger_thread([&]() {
    TestType merger(queues, output);
    merger.run();
  });

  std::vector<MarketDataEvent> result;
  while (output.pop([&](MarketDataEvent &&e) { result.push_back(e); })) {
  }

  merger_thread.join();

  REQUIRE(result.empty());
  REQUIRE(output.is_closed());
}

TEMPLATE_TEST_CASE("Merger - preserves event data through merge", "[Merger]",
                   FlatMerger<>, HierarchyMerger<>) {
  std::deque<BlockingQueue<MarketDataEvent>> queues(2);
  BlockingQueue<MarketDataEvent> output;

  MarketDataEvent e1;
  e1.ts_recv = 100 * NS;
  e1.sequence = 100;
  e1.action = Action::Add;
  e1.side = static_cast<Side>('B');
  e1.instrument_id = 123;

  MarketDataEvent e2;
  e2.ts_recv = 50 * NS;
  e2.sequence = 50;
  e2.action = Action::Cancel;
  e2.side = static_cast<Side>('A');
  e2.instrument_id = 456;

  queues[0].push(e1);
  queues[0].close();

  queues[1].push(e2);
  queues[1].close();

  std::thread merger_thread([&]() {
    TestType merger(queues, output);
    merger.run();
  });

  std::vector<MarketDataEvent> result;
  while (output.pop([&](MarketDataEvent &&e) { result.push_back(e); })) {
  }

  merger_thread.join();

  REQUIRE(result.size() == 2);

  REQUIRE(result[0].sequence == 50);
  REQUIRE(result[0].action == Action::Cancel);
  REQUIRE(result[0].side == static_cast<Side>('A'));
  REQUIRE(result[0].instrument_id == 456);

  REQUIRE(result[1].sequence == 100);
  REQUIRE(result[1].action == Action::Add);
  REQUIRE(result[1].side == static_cast<Side>('B'));
  REQUIRE(result[1].instrument_id == 123);
}

TEMPLATE_TEST_CASE("Merger - handles many events from multiple queues",
                   "[Merger]", FlatMerger<>, HierarchyMerger<>) {
  std::deque<BlockingQueue<MarketDataEvent>> queues(3);
  BlockingQueue<MarketDataEvent> output;

  for (int i = 0; i < 10; i += 3) {
    queues[0].push(make_event(i * NS, i));
  }
  queues[0].close();

  for (int i = 1; i < 10; i += 3) {
    queues[1].push(make_event(i * NS, i));
  }
  queues[1].close();

  for (int i = 2; i < 10; i += 3) {
    queues[2].push(make_event(i * NS, i));
  }
  queues[2].close();

  std::thread merger_thread([&]() {
    TestType merger(queues, output);
    merger.run();
  });

  std::vector<MarketDataEvent> result;
  while (output.pop([&](MarketDataEvent &&e) { result.push_back(e); })) {
  }

  merger_thread.join();

  REQUIRE(result.size() == 10);
  for (size_t i = 0; i < result.size(); ++i) {
    REQUIRE(result[i].ts_recv == static_cast<int64_t>(i) * NS);
  }
}
