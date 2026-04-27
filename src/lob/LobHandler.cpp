#include "lob/LobHandler.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <vector>

namespace cmf {

void LobHandler::printTopSnapshots(std::ostream& os, std::size_t top_n,
                                   std::size_t depth) const {
  std::vector<const Lob*> ranked;
  ranked.reserve(books_.size());
  for (const auto& kv : books_) ranked.push_back(&kv.second);

  std::sort(ranked.begin(), ranked.end(), [](const Lob* a, const Lob* b) {
    const auto ka = a->bidLevels() + a->askLevels() + a->orderCount();
    const auto kb = b->bidLevels() + b->askLevels() + b->orderCount();
    return ka > kb;
  });

  if (top_n > ranked.size()) top_n = ranked.size();
  for (std::size_t i = 0; i < top_n; ++i) {
    ranked[i]->printSnapshot(os, depth);
  }
}

void LobHandler::printBestBidAsk(std::ostream& os) const {
  std::vector<uint32_t> ids;
  ids.reserve(books_.size());
  for (const auto& kv : books_) ids.push_back(kv.first);
  std::sort(ids.begin(), ids.end());

  const auto prev_flags = os.flags();
  const auto prev_prec  = os.precision();

  const auto printPx = [&](int64_t px) {
    os << std::fixed << std::setprecision(9)
       << (static_cast<double>(px) /
           static_cast<double>(MarketDataEvent::kPriceScale));
  };

  for (uint32_t id : ids) {
    const Lob& l = books_.at(id);
    os << "inst=" << id << "  bid=";
    if (l.hasBid()) { printPx(l.bestBidPrice()); os << " x " << l.bestBidVolume(); }
    else            { os << "---"; }
    os << "  ask=";
    if (l.hasAsk()) { printPx(l.bestAskPrice()); os << " x " << l.bestAskVolume(); }
    else            { os << "---"; }
    os << '\n';
  }

  os.flags(prev_flags);
  os.precision(prev_prec);
}

}  // namespace cmf
