// main — CLI entry point for the back-tester data-ingestion layer.
//
// Standard task:
//   back-tester <path/to/file.mbo.json>
//     Parses one NDJSON file, prints first and last 10 events and a summary.
//
// Hard task:
//   back-tester <path/to/directory> [--strategy={flat|hierarchy|both}]
//                                   [--verbose]
//     Spawns one producer thread per *.mbo.json file in the directory,
//     builds a k-way merger (Flat / Hierarchy / both back-to-back), dispatches
//     the merged chronological stream through processMarketDataEvent, and
//     prints a benchmark line (total, elapsed, throughput, fingerprint).

#include "market_data/Consumer.hpp"
#include "market_data/HardTask.hpp"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct CliArgs {
  std::filesystem::path path;
  std::string strategy = "both"; // flat | hierarchy | both
  bool verbose = false;
};

void printUsage(const char *prog) {
  std::cerr << "Usage:\n"
            << "  " << prog << " <path-to-file.mbo.json>                  "
            << "# Standard task\n"
            << "  " << prog
            << " <path-to-dir> [--strategy=<flat|hierarchy|both>] "
               "[--verbose]\n"
            << "                                                       "
            << "# Hard task\n";
}

bool startsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

CliArgs parseArgs(int argc, const char *argv[]) {
  CliArgs a;
  if (argc < 2)
    throw std::runtime_error("missing path argument");
  a.path = argv[1];
  for (int i = 2; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (startsWith(arg, "--strategy=")) {
      a.strategy = std::string(arg.substr(std::strlen("--strategy=")));
    } else if (arg == "--verbose") {
      a.verbose = true;
    } else {
      throw std::runtime_error("unknown flag: " + std::string{arg});
    }
  }
  if (a.strategy != "flat" && a.strategy != "hierarchy" && a.strategy != "both")
    throw std::runtime_error("--strategy must be flat|hierarchy|both");
  return a;
}

int runStandard(const CliArgs &args) {
  constexpr std::size_t kHead = 10;
  constexpr std::size_t kTail = 10;
  const auto s = cmf::runStandardTask(args.path, kHead, kTail, std::cout);
  std::cout << "\nDone. Processed " << s.total << " messages.\n";
  return EXIT_SUCCESS;
}

int runHard(const CliArgs &args) {
  const auto files = cmf::listMboJsonFiles(args.path);
  if (files.empty()) {
    std::cerr << "No *.mbo.json files found in " << args.path << "\n";
    return EXIT_FAILURE;
  }

  std::cout << "Discovered " << files.size() << " MBO NDJSON files in "
            << args.path << ":\n";
  for (const auto &f : files)
    std::cout << "  " << f.filename().string() << "\n";
  std::cout << "\n";

  const bool do_flat = args.strategy == "flat" || args.strategy == "both";
  const bool do_hier = args.strategy == "hierarchy" || args.strategy == "both";

  cmf::BenchmarkResult flat_r, hier_r;
  if (do_flat) {
    flat_r = cmf::runHardTask(files, cmf::MergerKind::Flat, args.verbose);
    cmf::printBenchmarkResult(std::cout, flat_r);
  }
  if (do_hier) {
    hier_r = cmf::runHardTask(files, cmf::MergerKind::Hierarchy, args.verbose);
    cmf::printBenchmarkResult(std::cout, hier_r);
  }

  if (do_flat && do_hier) {
    std::cout << "\n=== Cross-check ===\n"
              << "Total match:       "
              << (flat_r.total == hier_r.total ? "yes" : "NO") << "\n"
              << "Fingerprint match: "
              << (flat_r.fingerprint == hier_r.fingerprint ? "yes" : "NO")
              << "\n";
    if (flat_r.total != hier_r.total ||
        flat_r.fingerprint != hier_r.fingerprint)
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

} // namespace

int main(int argc, const char *argv[]) {
  try {
    const auto args = parseArgs(argc, argv);

    std::error_code ec;
    if (!std::filesystem::exists(args.path, ec)) {
      std::cerr << "Path does not exist: " << args.path << "\n";
      return EXIT_FAILURE;
    }
    if (std::filesystem::is_directory(args.path, ec))
      return runHard(args);
    return runStandard(args);
  } catch (const std::exception &ex) {
    std::cerr << "back-tester: fatal: " << ex.what() << "\n";
    printUsage(argc > 0 ? argv[0] : "back-tester");
    return EXIT_FAILURE;
  }
}
