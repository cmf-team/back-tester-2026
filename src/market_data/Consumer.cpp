#include "market_data/Consumer.hpp"

#include "market_data/MmapMboSource.hpp"

#include <deque>
#include <iostream>
#include <ostream>
#include <vector>

namespace cmf {

void processMarketDataEvent(const MarketDataEvent &event) {
  std::cout << event << '\n';
}

IngestionSummary runStandardTask(const std::filesystem::path &path,
                                 std::size_t head_n, std::size_t tail_n,
                                 std::ostream &out) {
  MmapMboSource source(path);

  IngestionSummary s;
  std::vector<MarketDataEvent> head;
  head.reserve(head_n);
  std::deque<MarketDataEvent> tail;

  MarketDataEvent e;
  while (source.next(e)) {
    if (s.total == 0)
      s.first_ts = e.ts_recv;
    s.last_ts = e.ts_recv;
    ++s.total;

    if (head.size() < head_n)
      head.push_back(e);
    if (tail_n > 0) {
      tail.push_back(std::move(e));
      if (tail.size() > tail_n)
        tail.pop_front();
    }
  }

  out << "=== Summary ===\n"
      << "File:           " << path.string() << "\n"
      << "Total messages: " << s.total << "\n"
      << "First ts_recv:  " << s.first_ts << " ns\n"
      << "Last  ts_recv:  " << s.last_ts << " ns\n";

  out << "\n=== First " << head.size() << " events ===\n";
  for (const auto &e : head)
    out << e << '\n';

  out << "\n=== Last " << tail.size() << " events ===\n";
  for (const auto &e : tail)
    out << e << '\n';

  return s;
}

} // namespace cmf
