#include "ParserUtils.hpp"
#include "common/BasicTypes.hpp"
#include "common/LimitOrderBook.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using MarketDataEvent = cmf::MarketDataEvent;

std::vector<MarketDataEvent> Events;
LimitOrderBook LOB;

void printSnapshot(size_t event_count) {
  std::cout << "--- Snapshot at " << event_count << " events ---\n";
  std::cout << "  Best Bid: " << LOB.best_bid() * 1e-9
            << "  Best Ask: " << LOB.best_ask() * 1e-9 << "\n";
}

int main() {
  std::string path;
  std::cin >> path;

  auto start = std::chrono::steady_clock::now();
  cmf::parseNdjsonFile(path, Events);

  size_t total = Events.size();
  size_t interval = total / 4;

  for (size_t i = 0; i < total; ++i) {
    const auto &e = Events[i];

    if (e.action == "A")
      LOB.Add(e.order_id, e.price, e.size, e.side);
    else if (e.action == "C")
      LOB.Cancel(e.order_id, e.price, e.size, e.side);
    else if (e.action == "M")
      LOB.Modify(e.order_id, e.price, e.size, e.side);
    else if (e.action == "T")
      LOB.Trade(e.price, e.size, e.side);
    else if (e.action == "F")
      LOB.Fill(e.price, e.size, e.side);

    if (interval > 0 && (i + 1) % interval == 0 && (i + 1) != total)
      printSnapshot(i + 1);
  }

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  std::cout << "\n--- Final State ---\n";
  std::cout << "  Best Bid: " << LOB.best_bid() * 1e-9 << "\n";
  std::cout << "  Best Ask: " << LOB.best_ask() * 1e-9 << "\n";

  std::cout << "\n--- Performance ---\n";
  std::cout << "  Total events:    " << total << "\n";
  std::cout << "  Processing time: " << elapsed.count() << " s\n";
  std::cout << "  Throughput:      " << (size_t)(total / elapsed.count())
            << " events/sec\n";
}
