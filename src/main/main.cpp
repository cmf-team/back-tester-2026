// Back-tester entry point.
//
// Usage: back-tester <path-to-daily-zst-file>
//
// Reads one daily L3/MBO file (e.g. xeur-eobi-20260309.mbo.zst), converts each
// message into a MarketDataEvent and hands it off to processMarketDataEvent().
// This is the data-ingestion layer only - the LOB engine will consume these
// events in a later milestone.

#include "common/MarketDataEvent.hpp"
#include "ingestion/L3FileReader.hpp"

#include <array>
#include <cstddef>
#include <deque>
#include <exception>
#include <iostream>
#include <string>

using namespace cmf;

namespace {

// First / last events captured for the end-of-run report (10 each).
// We avoid storing every event to keep memory constant on full-day files.
constexpr std::size_t kSampleSize = 10;

std::array<MarketDataEvent, kSampleSize> g_firstEvents{};
std::size_t g_firstCount = 0;

std::deque<MarketDataEvent> g_lastEvents;

std::size_t g_totalMessages = 0;
NanoTime g_firstTs = 0;
NanoTime g_lastTs = 0;

// Consumer function required by the task. In this milestone it just prints a
// short line and bookkeeps first/last events; later it will forward to the
// LOB engine.
void processMarketDataEvent(const MarketDataEvent &order) {
  ++g_totalMessages;

  if (g_totalMessages == 1) {
    g_firstTs = order.ts_event;
  }
  g_lastTs = order.ts_event;

  if (g_firstCount < kSampleSize) {
    g_firstEvents[g_firstCount++] = order;
  }

  g_lastEvents.push_back(order);
  if (g_lastEvents.size() > kSampleSize) {
    g_lastEvents.pop_front();
  }
}

void printSection(const std::string &title) {
  std::cout << "\n=== " << title << " ===\n";
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << (argc > 0 ? argv[0] : "back-tester")
              << " <path-to-daily-zst-file>\n";
    return 2;
  }

  const std::string path = argv[1];

  try {
    const std::size_t n =
        readL3ZstFile(path, [](const MarketDataEvent &ev) {
          processMarketDataEvent(ev);
        });

    printSection("First " + std::to_string(g_firstCount) + " events");
    for (std::size_t i = 0; i < g_firstCount; ++i) {
      std::cout << '[' << i << "] " << g_firstEvents[i] << '\n';
    }

    printSection("Last " + std::to_string(g_lastEvents.size()) + " events");
    const std::size_t base =
        g_totalMessages >= g_lastEvents.size()
            ? g_totalMessages - g_lastEvents.size()
            : 0;
    for (std::size_t i = 0; i < g_lastEvents.size(); ++i) {
      std::cout << '[' << (base + i) << "] " << g_lastEvents[i] << '\n';
    }

    printSection("Summary");
    std::cout << "File              : " << path << '\n';
    std::cout << "Messages processed: " << n << '\n';
    if (n > 0) {
      std::cout << "First ts_event ns : " << g_firstTs << '\n';
      std::cout << "Last  ts_event ns : " << g_lastTs << '\n';
    }
  } catch (const std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << '\n';
    return 1;
  }

  return 0;
}
