// back-tester driver: feeds events from an IMarketDataSource into Statistics
// and prints the summary to stdout. The concrete source (file or folder) is
// chosen from the CLI argument.

#include "parser/FileMarketDataSource.hpp"
#include "parser/FolderMarketDataSource.hpp"
#include "parser/MarketDataEvent.hpp"
#include "stats/Statistics.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace cmf;

namespace {

// Templated on Source so next() resolves statically and the compiler can
// inline the parse loop. Source must satisfy IMarketDataSource's contract
// (i.e. expose `bool next(MarketDataEvent&)`).
template <class Source>
int runBacktest(Source& src) {
  Statistics      stats;
  MarketDataEvent ev;

  const auto    t0          = std::chrono::steady_clock::now();
  std::uint64_t events_read = 0;
  while (src.next(ev)) {
    stats.onEvent(ev);
    ++events_read;
  }
  const auto   t1         = std::chrono::steady_clock::now();
  const double sec        = std::chrono::duration<double>(t1 - t0).count();
  const double msgs_per_s = sec > 0.0 ? events_read / sec : 0.0;

  std::cerr << events_read << " events in "
            << std::fixed << std::setprecision(2) << sec << "s ("
            << std::setprecision(0) << msgs_per_s << " msg/s)\n";

  std::cout << stats;
  return 0;
}

} // namespace

int main(int argc, const char* argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "usage: " << (argc > 0 ? argv[0] : "back-tester")
                << " <file-or-folder>\n";
      return 2;
    }
    const std::filesystem::path path = argv[1];

    if (std::filesystem::is_directory(path)) {
      FolderMarketDataSource src(path);
      std::cerr << "parsing folder " << path << " ("
                << src.files().size() << " file(s))...\n";
      return runBacktest(src);
    }
    if (std::filesystem::is_regular_file(path)) {
      FileMarketDataSource src(path);
      std::cerr << "parsing file " << path << " ("
                << std::filesystem::file_size(path) / (1024 * 1024)
                << " MiB)...\n";
      return runBacktest(src);
    }
    throw std::runtime_error("not a file or directory: " + path.string());
  } catch (const std::exception& ex) {
    std::cerr << "back-tester: " << ex.what() << std::endl;
    return 1;
  }
}
