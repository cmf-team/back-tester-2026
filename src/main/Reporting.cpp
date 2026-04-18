#include "main/Reporting.hpp"

#include "parser/MarketDataEvent.hpp"

#include <iomanip>
#include <ostream>

namespace cmf {

void printBanner(std::ostream &os, const std::filesystem::path &path) {
  os << "back-tester HW-1 ingestion\n"
     << "  file: " << path << "\n"
     << "  size: "
     << (std::filesystem::file_size(path) / (1024.0 * 1024.0))
     << " MiB\n";
}

void printReport(std::ostream &os, const IngestionStats &stats,
                 const EventCollector &collector) {
  const auto &first = collector.firstEvents();
  const auto &last = collector.lastEvents();

  os << "\n--- first " << first.size() << " events ---\n";
  for (std::size_t i = 0; i < first.size(); ++i)
    os << "[" << std::setw(2) << i << "] " << first[i] << "\n";

  os << "\n--- last " << last.size() << " events ---\n";
  std::size_t idx =
      collector.total() > last.size() ? collector.total() - last.size() : 0;
  for (const auto &ev : last) {
    os << "[" << std::setw(2) << idx << "] " << ev << "\n";
    ++idx;
  }

  os << "\n--- summary ---\n";
  os << "total messages processed: " << stats.total_events << "\n";
  os << "first timestamp:          "
     << formatIso8601Nanos(collector.firstTimestamp()) << "\n";
  os << "last  timestamp:          "
     << formatIso8601Nanos(collector.lastTimestamp()) << "\n";
  os << "skipped (empty) lines:    " << stats.skipped_lines << "\n";
  os << "malformed lines:          " << stats.malformed_lines << "\n";
  os << "elapsed:                  " << std::fixed << std::setprecision(3)
     << stats.elapsed_seconds << " s";
  if (stats.elapsed_seconds > 0.0)
    os << "  (" << std::setprecision(0)
       << (static_cast<double>(stats.total_events) / stats.elapsed_seconds)
       << " msg/s)";
  os << "\n";
}

} // namespace cmf
