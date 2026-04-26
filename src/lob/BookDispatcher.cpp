#include "lob/BookDispatcher.hpp"

#include <algorithm>

namespace cmf {

BookDispatcher::BookDispatcher(DispatchOptions options) : options_(options) {}

void BookDispatcher::apply(const MarketDataEvent &event) {
  ++stats_.total_events;
  switch (event.action) {
  case MdAction::Add:
    ++stats_.adds;
    break;
  case MdAction::Cancel:
    ++stats_.cancels;
    break;
  case MdAction::Modify:
    ++stats_.modifies;
    break;
  case MdAction::Clear:
    ++stats_.clears;
    break;
  case MdAction::Trade:
    ++stats_.trades;
    break;
  case MdAction::Fill:
    ++stats_.fills;
    break;
  case MdAction::None:
    ++stats_.none;
    break;
  }

  bool ambiguous = false;
  const auto instrument_id = resolveInstrumentId(event, ambiguous);
  if (!instrument_id.has_value()) {
    if (ambiguous) {
      ++stats_.ambiguous_routes;
    } else {
      ++stats_.unresolved_routes;
    }
    return;
  }

  LimitOrderBook &book = getOrCreateBook(*instrument_id);
  const ApplyResult result = book.apply(event);
  if (result.missing_order) {
    ++stats_.missing_order_events;
  }
  if (result.ignored) {
    ++stats_.ignored_events;
  }
  maybeCaptureSnapshot(book, event);
}

std::vector<BookSummary> BookDispatcher::finalSummaries(std::size_t depth) const {
  std::vector<BookSummary> summaries;
  summaries.reserve(books_.size());
  for (const auto &[instrument_id, book] : books_) {
    (void)instrument_id;
    summaries.push_back(summarise(book, depth));
  }
  std::sort(summaries.begin(), summaries.end(),
            [](const BookSummary &lhs, const BookSummary &rhs) {
              const bool lhs_active =
                  lhs.best_bid.has_value() || lhs.best_ask.has_value();
              const bool rhs_active =
                  rhs.best_bid.has_value() || rhs.best_ask.has_value();
              if (lhs_active != rhs_active) {
                return lhs_active > rhs_active;
              }
              if (lhs.orders != rhs.orders) {
                return lhs.orders > rhs.orders;
              }
              return lhs.instrument_id < rhs.instrument_id;
            });
  return summaries;
}

LimitOrderBook &BookDispatcher::getOrCreateBook(std::uint32_t instrument_id) {
  const auto [it, inserted] = books_.try_emplace(instrument_id, instrument_id);
  if (inserted) {
    stats_.instruments = books_.size();
  }
  return it->second;
}

std::optional<std::uint32_t>
BookDispatcher::resolveInstrumentId(const MarketDataEvent &event,
                                    bool &ambiguous) const {
  if (event.instrument_id != 0) {
    return event.instrument_id;
  }
  if (event.order_id == 0) {
    return std::nullopt;
  }

  std::optional<std::uint32_t> resolved;
  for (const auto &[instrument_id, book] : books_) {
    if (!book.hasOrder(event.publisher_id, event.order_id)) {
      continue;
    }
    if (resolved.has_value()) {
      ambiguous = true;
      return std::nullopt;
    }
    resolved = instrument_id;
  }
  return resolved;
}

BookSummary BookDispatcher::summarise(const LimitOrderBook &book,
                                      std::size_t depth) const {
  return BookSummary{
      .instrument_id = book.instrumentId(),
      .orders = book.orderCount(),
      .bid_levels = book.bidLevelCount(),
      .ask_levels = book.askLevelCount(),
      .best_bid = book.bestBid(),
      .best_ask = book.bestAsk(),
      .snapshot = book.snapshotString(depth),
  };
}

void BookDispatcher::maybeCaptureSnapshot(const LimitOrderBook &book,
                                          const MarketDataEvent &event) {
  if (snapshots_.size() >= options_.max_snapshots) {
    return;
  }
  const bool is_first = stats_.total_events == 1;
  const bool hits_interval =
      options_.snapshot_every > 0 &&
      stats_.total_events % options_.snapshot_every == 0;
  if (!is_first && !hits_interval) {
    return;
  }
  snapshots_.push_back(CapturedSnapshot{
      .event_index = stats_.total_events,
      .ts_recv = event.ts_recv,
      .book = summarise(book, options_.snapshot_depth),
  });
  stats_.snapshots = snapshots_.size();
}

} // namespace cmf
