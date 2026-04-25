#include "ParserUtils.hpp"
#include "common/BasicTypes.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using MarketDataEvent = cmf::MarketDataEvent;

std::vector<MarketDataEvent> Events;

void processMarketDataEvent(const MarketDataEvent &order) {
  if ((int)Events.size() % 200000 == 0) {
    std::cout << order.ts_event << ' ' << order.order_id << ' ' << order.side
              << ' ' << order.price << ' ' << order.size << ' ' << order.action
              << std::endl;
  }
}

int main() {
  // path to .json
  std::string path;
  std::cin >> path;
  auto start = std::chrono::steady_clock::now();

  cmf::parseNdjsonFile(path, Events, processMarketDataEvent);

  std::cout << "Total messages processed: " << (int)Events.size() << std::endl;
  for (int i = 0; i < 10; ++i) {
    const MarketDataEvent *order = &Events[i];
    std::cout << order->ts_event << ' ' << order->order_id << ' ' << order->side
              << ' ' << order->price << ' ' << order->size << ' '
              << order->action << std::endl;
  }
  for (int i = (int)Events.size() - 1; i > (int)Events.size() - 11; --i) {
    const MarketDataEvent *order = &Events[i];
    std::cout << order->ts_event << ' ' << order->order_id << ' ' << order->side
              << ' ' << order->price << ' ' << order->ts_out << ' '
              << order->size << ' ' << order->action << std::endl;
  }

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;
  std::cout << "Throughput: " << (int)Events.size() / elapsed.count()
            << " messages per second" << std::endl;
}
