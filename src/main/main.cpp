#include "common/SpscQueue.hpp"
#include "common/MarketDataEvent.hpp"
#include <thread>
#include <iostream>

using namespace cmf;

int main() {
  SpscQueue<MarketDataEvent, 1024> q;
  bool done = false;
  std::size_t count = 0;

  // Consumer thread
  std::thread consumer([&] {
      MarketDataEvent e;
      while (!done || count < 100) {
          if (q.try_pop(e)) {  // или pop(e) с блокировкой
              count++;
          }
      }
      std::cout << "Consumer received: " << count << "\n";
  });

  // Producer thread
  for (std::size_t i = 0; i < 100; ++i) {
    MarketDataEvent e{};
    e.ts_recv = i * 1000;
    q.push(e);
  }
  done = true;

  consumer.join();
  return 0;
}