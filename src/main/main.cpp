// back-tester driver: opens one source per CLI arg (file or folder), merges
// them via LoserTreeMerger over a VariantSource, and feeds the merged stream
// into Statistics. VariantSource replaces virtual dispatch through
// IMarketDataSource* with std::visit on a closed set of concrete types.

#include "merge/LoserTreeMerger.hpp"
#include "parser/FileMarketDataSource.hpp"
#include "parser/FolderMarketDataSource.hpp"
#include "parser/MarketDataEvent.hpp"
#include "parser/VariantSource.hpp"
#include "stats/Statistics.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace cmf;

namespace {

using AnySource = VariantSource<FileMarketDataSource, FolderMarketDataSource>;

// Templated on Source so next() resolves statically and the compiler can
// inline the parse loop.
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

AnySource openSource(const std::filesystem::path& path) {
  if (std::filesystem::is_directory(path))
    return AnySource{FolderMarketDataSource(path)};
  if (std::filesystem::is_regular_file(path))
    return AnySource{FileMarketDataSource(path)};
  throw std::runtime_error("not a file or directory: " + path.string());
}

} // namespace

int main(int argc, const char* argv[]) {
  try {
    if (argc < 2) {
      std::cerr << "usage: " << (argc > 0 ? argv[0] : "back-tester")
                << " <file-or-folder> [<file-or-folder> ...]\n";
      return 2;
    }

    std::vector<AnySource> sources;
    // reserve() is mandatory here: the ptrs vector below holds pointers into
    // `sources`, so any reallocation would dangle them.
    sources.reserve(static_cast<std::size_t>(argc - 1));
    for (int i = 1; i < argc; ++i)
      sources.push_back(openSource(argv[i]));

    std::vector<AnySource*> ptrs;
    ptrs.reserve(sources.size());
    for (auto& s : sources) ptrs.push_back(&s);

    std::cerr << "merging " << sources.size() << " source(s)...\n";

    LoserTreeMerger merger(std::move(ptrs));
    return runBacktest(merger);
  } catch (const std::exception& ex) {
    std::cerr << "back-tester: " << ex.what() << std::endl;
    return 1;
  }
}
