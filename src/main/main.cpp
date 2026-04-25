

#include "common/EventParser.hpp"
#include "common/MarketDataDispatcher.hpp"
#include "common/MarketDataEvent.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace cmf;

namespace {
  constexpr std::size_t kIntermediateSnapshots = 3;

  constexpr std::size_t kSnapshotDepth = 10;

  constexpr std::size_t kFinalQuotesLimit = 20;

  void printPerStats(std::size_t total, std::size_t parsed, std::size_t lob_changes,
                     double seconds) {
    std::cout << "\n=== Performance ===\n";
    std::cout << "Total lines:         " << total << "\n";
    std::cout << "Parsed events:       " << parsed << "\n";
    std::cout << "Book-changing evts:  " << lob_changes << "\n";
    std::cout << "Wall time:           " << seconds << " s\n";
    if (seconds > 0) {
      std::cout << "Throughput:          "
          << static_cast<long long>(parsed / seconds) << " events/sec\n";
    }
  }
} // namespace

int main(int argc, const char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: " << (argc > 0 ? argv[0] : "back-tester")
          << " <path/to/L3-file.json>\n";
      return 1;
    }

    const std::string path = argv[1];
    std::ifstream in(path);
    if (!in) {
      std::cerr << "Cannot open file: " << path << "\n";
      return 1;
    }

    MarketDataDispatcher dispatcher;

    std::size_t total = 0;
    std::size_t parsed = 0;
    std::size_t skipped = 0;
    std::size_t lob_changes = 0;


    const std::vector<std::size_t> snapshot_marks = {
      100'000, 500'000, 2'000'000
    };


    std::uint32_t demo_instrument = 0;

    auto t0 = std::chrono::steady_clock::now();

    std::string line;
    line.reserve(512);
    while (std::getline(in, line)) {
      ++total;
      auto opt = parseEvent(line);
      if (!opt) {
        ++skipped;
        continue;
      }
      ++parsed;
      const auto &ev = *opt;

      bool changed = dispatcher.dispatch(ev);
      if (changed) {
        ++lob_changes;
      }

      if (demo_instrument == 0 && ev.instrument_id != 0 &&
          ev.action == Action::Add) {
        demo_instrument = ev.instrument_id;
      }


      for (std::size_t mark: snapshot_marks) {
        if (parsed == mark && demo_instrument != 0) {
          std::cout << "\n--- Snapshot @ event #" << parsed << " ---\n";
          dispatcher.printSnapshot(std::cout, demo_instrument, kSnapshotDepth);
          break;
        }
      }
    }

    auto t1 = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(t1 - t0).count();


    if (demo_instrument != 0) {
      std::cout << "\n--- Final snapshot ---\n";
      dispatcher.printSnapshot(std::cout, demo_instrument, kSnapshotDepth);
    }


    std::cout << "\nKnown instruments: " << dispatcher.instrumentCount() << "\n";
    if (dispatcher.instrumentCount() <= kFinalQuotesLimit) {
      dispatcher.printAllBestQuotes(std::cout);
    } else {
      std::cout << "(too many to print all; showing demo instrument only)\n";
    }

    std::cout << "Skipped lines: " << skipped << "\n";

    printPerStats(total, parsed, lob_changes, seconds);
    return 0;
  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << "\n";
    return 1;
  }
}
