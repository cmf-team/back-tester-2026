// HW-1 ingestion driver:
//
//   back-tester <path-to-daily-NDJSON>
//
// Wires the pieces together: parse CLI, create the runner with the
// HW-1 consumer, run it, print the report. All logic lives in
// EventCollector / IngestionRunner / Reporting / processMarketDataEvent.

#include "main/EventCollector.hpp"
#include "main/IngestionRunner.hpp"
#include "main/Reporting.hpp"
#include "main/processMarketDataEvent.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, const char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "usage: " << (argc > 0 ? argv[0] : "back-tester")
                << " <daily-ndjson-file>\n";
      return 2;
    }
    const std::filesystem::path path = argv[1];
    if (!std::filesystem::is_regular_file(path))
      throw std::runtime_error("not a regular file: " + path.string());

    cmf::printBanner(std::cerr, path);

    cmf::IngestionRunner runner(path, &cmf::processMarketDataEvent);
    const auto stats = runner.run();

    cmf::printReport(std::cout, stats, cmf::defaultEventCollector());
  } catch (const std::exception &ex) {
    std::cerr << "back-tester: " << ex.what() << std::endl;
    return 1;
  }
  return 0;
}
