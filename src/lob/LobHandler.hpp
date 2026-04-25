// Per-instrument router: takes a merged MarketDataEvent stream and dispatches
// to the right Lob. Shape mirrors Statistics so both can sit in the driver's
// tight loop.

#pragma once

#include "lob/Lob.hpp"
#include "parser/MarketDataEvent.hpp"

#include <cstdint>
#include <iosfwd>
#include <unordered_map>

namespace cmf {

class LobHandler {
 public:
  // Header-inline so the driver's `while (src.next(ev)) lob.onEvent(ev)`
  // inlines the switch and the hash lookup.
  void onEvent(const MarketDataEvent& ev) noexcept {
    switch (ev.action) {
      case Action::Add:
        bookFor(ev.instrument_id).add(ev.order_id, ev.side, ev.price, ev.size);
        break;
      case Action::Modify:
        bookFor(ev.instrument_id)
            .modify(ev.order_id, ev.side, ev.price, ev.size);
        break;
      case Action::Cancel:
        bookFor(ev.instrument_id).cancel(ev.order_id, ev.size);
        break;
      case Action::Clear:
        bookFor(ev.instrument_id).clear();
        break;
      case Action::Trade:
      case Action::Fill:
      case Action::None:
      default:
        // Per Databento MBO spec these don't mutate the book.
        return;
    }
  }

  void reset() noexcept { books_.clear(); }

  std::size_t bookCount() const noexcept { return books_.size(); }
  const std::unordered_map<uint32_t, Lob>& books() const noexcept {
    return books_;
  }

  const Lob* book(uint32_t instrument_id) const noexcept {
    auto it = books_.find(instrument_id);
    return it == books_.end() ? nullptr : &it->second;
  }

  void setDefaultConfig(LobConfig cfg) noexcept { default_cfg_ = cfg; }
  void setConfig(uint32_t instrument_id, LobConfig cfg) noexcept {
    cfg_overrides_[instrument_id] = cfg;
  }

  // Print up to `top_n` busiest books (ranked by populated levels + resting
  // orders) at `depth` levels each.
  void printTopSnapshots(std::ostream& os, std::size_t top_n = 3,
                         std::size_t depth = 10) const;

  // One line per instrument (sorted by instrument_id): best bid and best ask
  // at end-of-stream. Empty side prints as "---".
  void printBestBidAsk(std::ostream& os) const;

 private:
  Lob& bookFor(uint32_t instrument_id) noexcept {
    auto it = books_.find(instrument_id);
    if (it != books_.end()) return it->second;
    LobConfig cfg = default_cfg_;
    if (auto ov = cfg_overrides_.find(instrument_id);
        ov != cfg_overrides_.end()) {
      cfg = ov->second;
    }
    // try_emplace constructs Lob in place — avoids a 128 KB move.
    return books_.try_emplace(instrument_id, instrument_id, cfg)
        .first->second;
  }

  LobConfig                                default_cfg_{};
  std::unordered_map<uint32_t, LobConfig>  cfg_overrides_;
  std::unordered_map<uint32_t, Lob>        books_;
};

}  // namespace cmf
