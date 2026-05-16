#include "TestSupport.hpp"

#include "book/BookManager.hpp"

namespace md::test {

namespace {

std::int64_t P(std::int64_t integer_price) {
    return integer_price * 1'000'000'000LL;
}

MarketDataEvent event(
    Action action,
    std::uint64_t instrument_id,
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    MarketDataEvent result;
    result.action = action;
    result.instrument_id = instrument_id;
    result.order_id = order_id;
    result.side = side;
    result.price = price;
    result.size = size;
    return result;
}

MarketDataEvent add(
    std::uint64_t instrument_id,
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    return event(Action::Add, instrument_id, order_id, side, price, size);
}

MarketDataEvent cancel(std::uint64_t instrument_id, std::uint64_t order_id, std::uint64_t size) {
    return event(Action::Cancel, instrument_id, order_id, Side::None, 0, size);
}

MarketDataEvent modify(
    std::uint64_t instrument_id,
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    return event(Action::Modify, instrument_id, order_id, side, price, size);
}

MarketDataEvent clearEvent(std::uint64_t instrument_id) {
    return event(Action::Clear, instrument_id, 0, Side::None, 0, 0);
}

} // namespace

void testBookManagerCreatesSeparateBooksPerInstrument() {
    BookManager manager;

    manager.apply(add(1, 10, Side::Bid, P(100), 10));
    manager.apply(add(2, 20, Side::Bid, P(200), 20));

    const auto* first = manager.findBook(1);
    const auto* second = manager.findBook(2);
    require(first != nullptr, "first instrument book exists");
    require(second != nullptr, "second instrument book exists");
    require(manager.instrumentCount() == 2, "manager created two instrument books");
    require(first->bestBid() == P(100), "first instrument best bid");
    require(first->volumeAt(Side::Bid, P(100)) == 10, "first instrument bid volume");
    require(second->bestBid() == P(200), "second instrument best bid");
    require(second->volumeAt(Side::Bid, P(200)) == 20, "second instrument bid volume");
    require(manager.processedEvents() == 2, "manager processed two events");
}

void testBookManagerRoutesCancelByExplicitInstrumentId() {
    BookManager manager;

    manager.apply(add(1, 42, Side::Bid, P(100), 10));
    manager.apply(cancel(1, 42, 4));

    const auto* book = manager.findBook(1);
    require(book != nullptr, "explicit cancel book exists");
    require(book->volumeAt(Side::Bid, P(100)) == 6, "explicit cancel routed to instrument book");
    require(book->bestBid() == P(100), "explicit cancel keeps best bid");
    require(manager.unresolvedEvents() == 0, "explicit cancel is resolved");
}

void testBookManagerResolvesCancelByOrderIdWhenInstrumentMissing() {
    BookManager manager;

    manager.apply(add(1, 42, Side::Bid, P(100), 10));
    manager.apply(cancel(0, 42, 4));

    const auto* book = manager.findBook(1);
    require(book != nullptr, "resolved cancel book exists");
    require(book->volumeAt(Side::Bid, P(100)) == 6, "missing-instrument cancel uses order mapping");
    require(book->bestBid() == P(100), "resolved cancel keeps best bid");
    require(manager.unresolvedEvents() == 0, "missing-instrument cancel is resolved");
}

void testBookManagerResolvesModifyByOrderIdWhenInstrumentMissing() {
    BookManager manager;

    manager.apply(add(1, 42, Side::Bid, P(100), 10));
    manager.apply(modify(0, 42, Side::Bid, P(101), 8));

    const auto* book = manager.findBook(1);
    require(book != nullptr, "resolved modify book exists");
    require(book->volumeAt(Side::Bid, P(100)) == 0, "resolved modify removes old bid volume");
    require(book->volumeAt(Side::Bid, P(101)) == 8, "resolved modify adds new bid volume");
    require(book->bestBid() == P(101), "resolved modify updates best bid");
    require(manager.unresolvedEvents() == 0, "missing-instrument modify is resolved");
}

void testBookManagerClearRemovesOrderMappingsForInstrument() {
    BookManager manager;

    manager.apply(add(1, 42, Side::Bid, P(100), 10));
    manager.apply(clearEvent(1));
    manager.apply(cancel(0, 42, 1));

    const auto* book = manager.findBook(1);
    require(book != nullptr, "cleared book still exists");
    require(!book->bestBid().has_value(), "clear removes best bid");
    require(book->restingOrderCount() == 0, "clear removes resting orders");
    require(manager.unresolvedEvents() == 1, "post-clear missing-instrument cancel is unresolved");
}

void testBookManagerUnknownOrderWithoutInstrumentIsUnresolved() {
    BookManager manager;

    manager.apply(cancel(0, 999, 1));

    require(manager.instrumentCount() == 0, "unknown missing-instrument order creates no book");
    require(manager.unresolvedEvents() == 1, "unknown missing-instrument order is unresolved");
    require(manager.processedEvents() == 1, "unknown missing-instrument order is still processed");
}

} // namespace md::test
