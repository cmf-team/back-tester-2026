#include "book/BookManager.hpp"

#include <algorithm>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace md {
namespace {

bool hasValidSide(Side side) {
    return side == Side::Bid || side == Side::Ask;
}

bool hasValidPrice(std::int64_t price) {
    return price != std::numeric_limits<std::int64_t>::max();
}

bool hasValidRestingState(const MarketDataEvent& event) {
    return event.order_id != 0
        && hasValidSide(event.side)
        && hasValidPrice(event.price)
        && event.size > 0;
}

std::vector<std::uint64_t> sortedInstrumentIds(
    const std::unordered_map<std::uint64_t, LimitOrderBook>& books
) {
    std::vector<std::uint64_t> ids;
    ids.reserve(books.size());
    for (const auto& [instrument_id, book] : books) {
        (void)book;
        ids.push_back(instrument_id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::string formatOptionalPrice(std::optional<std::int64_t> price) {
    return price.has_value() ? formatPrice(*price) : "<none>";
}

std::vector<PriceLevelSnapshot> makeLevelSnapshots(
    const std::vector<std::pair<std::int64_t, std::uint64_t>>& levels
) {
    std::vector<PriceLevelSnapshot> snapshots;
    snapshots.reserve(levels.size());
    for (const auto& [price, size] : levels) {
        snapshots.push_back(PriceLevelSnapshot{
            .price = price,
            .size = size,
        });
    }
    return snapshots;
}

} // namespace

void BookManager::apply(const MarketDataEvent& event) {
    ++processed_events_;

    const auto instrument_id = resolveInstrumentId(event);
    if (instrument_id == 0) {
        ++unresolved_events_;
        return;
    }

    MarketDataEvent routed_event = event;
    routed_event.instrument_id = instrument_id;

    removePreviousInstrumentMapping(routed_event, instrument_id);

    auto& book = getOrCreateBook(instrument_id);
    book.apply(routed_event);
    updateOrderMapping(routed_event, book);
}

const LimitOrderBook* BookManager::findBook(std::uint64_t instrument_id) const {
    const auto it = books_by_instrument_.find(instrument_id);
    if (it == books_by_instrument_.end()) {
        return nullptr;
    }

    return &it->second;
}

LimitOrderBook& BookManager::getOrCreateBook(std::uint64_t instrument_id) {
    auto [it, inserted] = books_by_instrument_.try_emplace(instrument_id, instrument_id);
    (void)inserted;
    return it->second;
}

std::size_t BookManager::instrumentCount() const noexcept {
    return books_by_instrument_.size();
}

std::size_t BookManager::processedEvents() const noexcept {
    return processed_events_;
}

std::size_t BookManager::unresolvedEvents() const noexcept {
    return unresolved_events_;
}

void BookManager::printSnapshot(std::ostream& out, std::size_t depth) const {
    out << "BookManager snapshot"
        << " instruments=" << instrumentCount()
        << " processed_events=" << processed_events_
        << " unresolved_events=" << unresolved_events_ << '\n';

    for (const auto instrument_id : sortedInstrumentIds(books_by_instrument_)) {
        books_by_instrument_.at(instrument_id).printSnapshot(out, depth);
    }
}

std::string BookManager::stableStateDigest() const {
    std::ostringstream out;
    out << "processed_events=" << processed_events_
        << ";unresolved_events=" << unresolved_events_
        << ";instrument_count=" << instrumentCount();

    for (const auto instrument_id : sortedInstrumentIds(books_by_instrument_)) {
        const auto& book = books_by_instrument_.at(instrument_id);
        out << "|instrument=" << instrument_id
            << ",orders=" << book.restingOrderCount()
            << ",best_bid=" << formatOptionalPrice(book.bestBid())
            << ",best_ask=" << formatOptionalPrice(book.bestAsk());

        out << ",bids=[";
        bool first = true;
        for (const auto& [price, volume] : book.bidLevels()) {
            if (!first) {
                out << ';';
            }
            first = false;
            out << formatPrice(price) << 'x' << volume;
        }
        out << "]";

        out << ",asks=[";
        first = true;
        for (const auto& [price, volume] : book.askLevels()) {
            if (!first) {
                out << ';';
            }
            first = false;
            out << formatPrice(price) << 'x' << volume;
        }
        out << "]";
    }

    return out.str();
}

BookManagerSnapshot BookManager::snapshot(
    std::size_t event_count,
    std::uint64_t timestamp,
    std::size_t depth
) const {
    BookManagerSnapshot snapshot;
    snapshot.event_count = event_count;
    snapshot.timestamp = timestamp;
    snapshot.processed_events = processed_events_;
    snapshot.unresolved_events = unresolved_events_;
    snapshot.instruments.reserve(books_by_instrument_.size());

    for (const auto instrument_id : sortedInstrumentIds(books_by_instrument_)) {
        const auto& book = books_by_instrument_.at(instrument_id);
        snapshot.instruments.push_back(InstrumentBookSnapshot{
            .instrument_id = instrument_id,
            .resting_orders = book.restingOrderCount(),
            .best_bid = book.bestBid(),
            .best_ask = book.bestAsk(),
            .bids = makeLevelSnapshots(book.bidLevels(depth)),
            .asks = makeLevelSnapshots(book.askLevels(depth)),
        });
    }

    return snapshot;
}

void BookManager::printFinalBestBidAsk(std::ostream& out) const {
    out << "instrument_count=" << instrumentCount() << '\n'
        << "processed_events=" << processed_events_ << '\n'
        << "unresolved_events=" << unresolved_events_ << '\n';
    for (const auto instrument_id : sortedInstrumentIds(books_by_instrument_)) {
        const auto& book = books_by_instrument_.at(instrument_id);
        out << "instrument_id=" << instrument_id
            << " resting_orders=" << book.restingOrderCount()
            << " best_bid=" << formatOptionalPrice(book.bestBid())
            << " best_ask=" << formatOptionalPrice(book.bestAsk()) << '\n';
    }
}

std::uint64_t BookManager::resolveInstrumentId(const MarketDataEvent& event) const {
    if (event.instrument_id != 0) {
        return event.instrument_id;
    }

    if (event.order_id == 0) {
        return 0;
    }

    const auto it = order_to_instrument_.find(event.order_id);
    if (it == order_to_instrument_.end()) {
        return 0;
    }

    return it->second;
}

void BookManager::updateOrderMapping(const MarketDataEvent& event, const LimitOrderBook& book) {
    switch (event.action) {
        case Action::Add:
        case Action::Modify:
            if (event.order_id != 0 && book.containsOrder(event.order_id)) {
                order_to_instrument_[event.order_id] = event.instrument_id;
            }
            break;
        case Action::Cancel:
            if (event.order_id == 0) {
                break;
            }
            if (book.containsOrder(event.order_id)) {
                order_to_instrument_[event.order_id] = event.instrument_id;
            } else {
                eraseOrderMappingIfMatches(event.order_id, event.instrument_id);
            }
            break;
        case Action::Clear:
            eraseOrderMappingsForInstrument(event.instrument_id);
            break;
        case Action::Trade:
        case Action::Fill:
        case Action::None:
            break;
    }
}

void BookManager::eraseOrderMappingIfMatches(std::uint64_t order_id, std::uint64_t instrument_id) {
    const auto it = order_to_instrument_.find(order_id);
    if (it != order_to_instrument_.end() && it->second == instrument_id) {
        order_to_instrument_.erase(it);
    }
}

void BookManager::eraseOrderMappingsForInstrument(std::uint64_t instrument_id) {
    for (auto it = order_to_instrument_.begin(); it != order_to_instrument_.end();) {
        if (it->second == instrument_id) {
            it = order_to_instrument_.erase(it);
        } else {
            ++it;
        }
    }
}

void BookManager::removePreviousInstrumentMapping(
    const MarketDataEvent& event,
    std::uint64_t target_instrument_id
) {
    if ((event.action != Action::Add && event.action != Action::Modify)
        || !hasValidRestingState(event)) {
        return;
    }

    const auto it = order_to_instrument_.find(event.order_id);
    if (it == order_to_instrument_.end() || it->second == target_instrument_id) {
        return;
    }

    const auto previous_instrument_id = it->second;
    const auto book_it = books_by_instrument_.find(previous_instrument_id);
    if (book_it != books_by_instrument_.end()) {
        MarketDataEvent cancel_previous = event;
        cancel_previous.action = Action::Cancel;
        cancel_previous.instrument_id = previous_instrument_id;
        cancel_previous.size = 0;
        book_it->second.apply(cancel_previous);
    }

    order_to_instrument_.erase(it);
}

} // namespace md
