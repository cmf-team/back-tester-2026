#include "TestSupport.hpp"

#include "book/LimitOrderBook.hpp"

namespace md::test {

namespace {

std::int64_t P(std::int64_t integer_price) {
    return integer_price * 1'000'000'000LL;
}

MarketDataEvent add(
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    MarketDataEvent event;
    event.order_id = order_id;
    event.side = side;
    event.price = price;
    event.size = size;
    event.action = Action::Add;
    event.instrument_id = 42;
    return event;
}

MarketDataEvent cancel(std::uint64_t order_id, std::uint64_t size) {
    MarketDataEvent event;
    event.order_id = order_id;
    event.size = size;
    event.action = Action::Cancel;
    event.instrument_id = 42;
    return event;
}

MarketDataEvent modify(
    std::uint64_t order_id,
    Side side,
    std::int64_t price,
    std::uint64_t size
) {
    MarketDataEvent event;
    event.order_id = order_id;
    event.side = side;
    event.price = price;
    event.size = size;
    event.action = Action::Modify;
    event.instrument_id = 42;
    return event;
}

MarketDataEvent clearEvent() {
    MarketDataEvent event;
    event.action = Action::Clear;
    event.instrument_id = 42;
    return event;
}

MarketDataEvent trade(std::int64_t price, std::uint64_t size) {
    MarketDataEvent event;
    event.price = price;
    event.size = size;
    event.action = Action::Trade;
    event.instrument_id = 42;
    return event;
}

MarketDataEvent fill(std::uint64_t order_id, std::uint64_t size) {
    MarketDataEvent event;
    event.order_id = order_id;
    event.size = size;
    event.action = Action::Fill;
    event.instrument_id = 42;
    return event;
}

} // namespace

void testLimitOrderBookStartsEmpty() {
    LimitOrderBook book{42};

    require(!book.bestBid().has_value(), "new book has no best bid");
    require(!book.bestAsk().has_value(), "new book has no best ask");
    require(book.volumeAt(Side::Bid, P(100)) == 0, "new book has no bid volume");
    require(book.volumeAt(Side::Ask, P(100)) == 0, "new book has no ask volume");
    require(book.restingOrderCount() == 0, "new book has no resting orders");
    require(book.skippedUnknownOrderCount() == 0, "new book has no skipped unknown orders");
    require(book.tradeCount() == 0, "new book has no trades");
    require(book.fillCount() == 0, "new book has no fills");
}

void testLimitOrderBookReportsInstrumentId() {
    LimitOrderBook book{42};

    require(book.instrumentId() == 42, "book reports instrument id");
}

void testLobAddSingleBid() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));

    require(book.bestBid() == P(100), "single bid becomes best bid");
    require(!book.bestAsk().has_value(), "single bid leaves ask side empty");
    require(book.volumeAt(Side::Bid, P(100)) == 10, "single bid volume");
    require(book.restingOrderCount() == 1, "single bid resting count");
}

void testLobAddSingleAsk() {
    LimitOrderBook book{42};

    book.apply(add(2, Side::Ask, P(105), 7));

    require(!book.bestBid().has_value(), "single ask leaves bid side empty");
    require(book.bestAsk() == P(105), "single ask becomes best ask");
    require(book.volumeAt(Side::Ask, P(105)) == 7, "single ask volume");
    require(book.restingOrderCount() == 1, "single ask resting count");
}

void testLobAddAggregatesSameBidPrice() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(add(2, Side::Bid, P(100), 15));

    require(book.bestBid() == P(100), "aggregated bid price remains best bid");
    require(book.volumeAt(Side::Bid, P(100)) == 25, "bid volume aggregates");
    require(book.restingOrderCount() == 2, "two bid orders rest");
}

void testLobAddAggregatesSameAskPrice() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Ask, P(105), 10));
    book.apply(add(2, Side::Ask, P(105), 15));

    require(book.bestAsk() == P(105), "aggregated ask price remains best ask");
    require(book.volumeAt(Side::Ask, P(105)) == 25, "ask volume aggregates");
    require(book.restingOrderCount() == 2, "two ask orders rest");
}

void testLobBestBidIsHighestBidPrice() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(add(2, Side::Bid, P(101), 5));
    book.apply(add(3, Side::Bid, P(99), 20));

    require(book.bestBid() == P(101), "best bid is highest bid price");
}

void testLobBestAskIsLowestAskPrice() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Ask, P(105), 10));
    book.apply(add(2, Side::Ask, P(104), 5));
    book.apply(add(3, Side::Ask, P(106), 20));

    require(book.bestAsk() == P(104), "best ask is lowest ask price");
}

void testLobDuplicateAddReplacesOldOrder() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(add(1, Side::Bid, P(101), 8));

    require(book.volumeAt(Side::Bid, P(100)) == 0, "duplicate add removes old price volume");
    require(book.volumeAt(Side::Bid, P(101)) == 8, "duplicate add inserts replacement volume");
    require(book.bestBid() == P(101), "replacement order becomes best bid");
    require(book.restingOrderCount() == 1, "duplicate add keeps one resting order");
}

void testLobCancelPartial() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(cancel(1, 4));

    require(book.volumeAt(Side::Bid, P(100)) == 6, "partial cancel subtracts bid volume");
    require(book.bestBid() == P(100), "partial cancel leaves bid level live");
    require(book.restingOrderCount() == 1, "partial cancel leaves order resting");
}

void testLobCancelFullWithSize() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(cancel(1, 10));

    require(book.volumeAt(Side::Bid, P(100)) == 0, "full cancel removes bid volume");
    require(!book.bestBid().has_value(), "full cancel clears best bid");
    require(book.restingOrderCount() == 0, "full cancel removes order");
}

void testLobCancelFullWithZeroSize() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Ask, P(105), 7));
    book.apply(cancel(1, 0));

    require(book.volumeAt(Side::Ask, P(105)) == 0, "zero-size cancel removes ask volume");
    require(!book.bestAsk().has_value(), "zero-size cancel clears best ask");
    require(book.restingOrderCount() == 0, "zero-size cancel removes order");
}

void testLobCancelLargerThanRestingSizeIsCapped() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(cancel(1, 999));

    require(book.volumeAt(Side::Bid, P(100)) == 0, "oversized cancel removes capped bid volume");
    require(!book.bestBid().has_value(), "oversized cancel clears best bid");
    require(book.restingOrderCount() == 0, "oversized cancel removes order without underflow");
}

void testLobCancelUnknownOrderIsNoop() {
    LimitOrderBook book{42};

    book.apply(cancel(999, 10));

    require(!book.bestBid().has_value(), "unknown cancel leaves bid side empty");
    require(!book.bestAsk().has_value(), "unknown cancel leaves ask side empty");
    require(book.restingOrderCount() == 0, "unknown cancel leaves no resting orders");
    require(book.skippedUnknownOrderCount() == 1, "unknown cancel increments skipped count");
}

void testLobCancelRemovesEmptyPriceLevel() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(add(2, Side::Bid, P(99), 5));
    book.apply(cancel(1, 10));

    require(book.volumeAt(Side::Bid, P(100)) == 0, "full cancel removes empty price level");
    require(book.bestBid() == P(99), "best bid skips removed empty price level");
    require(book.restingOrderCount() == 1, "only lower bid remains resting");
}

void testLobModifySizeSamePrice() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(modify(1, Side::Bid, P(100), 20));

    require(book.volumeAt(Side::Bid, P(100)) == 20, "modify updates same-price bid size");
    require(book.bestBid() == P(100), "same-price modify keeps best bid");
    require(book.restingOrderCount() == 1, "same-price modify keeps one resting order");
}

void testLobModifyPriceLevel() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(modify(1, Side::Bid, P(101), 8));

    require(book.volumeAt(Side::Bid, P(100)) == 0, "modify removes old bid price volume");
    require(book.volumeAt(Side::Bid, P(101)) == 8, "modify adds new bid price volume");
    require(book.bestBid() == P(101), "modify price level updates best bid");
    require(book.restingOrderCount() == 1, "price modify keeps one resting order");
}

void testLobModifySideChange() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(modify(1, Side::Ask, P(105), 7));

    require(book.volumeAt(Side::Bid, P(100)) == 0, "modify side change removes bid volume");
    require(book.volumeAt(Side::Ask, P(105)) == 7, "modify side change adds ask volume");
    require(!book.bestBid().has_value(), "modify side change clears bid side");
    require(book.bestAsk() == P(105), "modify side change sets best ask");
    require(book.restingOrderCount() == 1, "side modify keeps one resting order");
}

void testLobModifyUnknownOrderWithFullStateBecomesAdd() {
    LimitOrderBook book{42};

    book.apply(modify(1, Side::Bid, P(100), 10));

    require(book.volumeAt(Side::Bid, P(100)) == 10, "unknown full-state modify adds bid volume");
    require(book.bestBid() == P(100), "unknown full-state modify sets best bid");
    require(book.restingOrderCount() == 1, "unknown full-state modify creates resting order");
    require(book.skippedUnknownOrderCount() == 0, "unknown full-state modify is not skipped");
}

void testLobClearEmptiesBook() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(add(2, Side::Ask, P(105), 7));
    book.apply(clearEvent());

    require(!book.bestBid().has_value(), "clear removes best bid");
    require(!book.bestAsk().has_value(), "clear removes best ask");
    require(book.volumeAt(Side::Bid, P(100)) == 0, "clear removes bid volume");
    require(book.volumeAt(Side::Ask, P(105)) == 0, "clear removes ask volume");
    require(book.restingOrderCount() == 0, "clear removes resting orders");
}

void testLobTradeIsExplicitNoop() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(trade(P(100), 5));

    require(book.volumeAt(Side::Bid, P(100)) == 10, "trade leaves bid volume unchanged");
    require(book.bestBid() == P(100), "trade leaves best bid unchanged");
    require(book.restingOrderCount() == 1, "trade leaves resting order count unchanged");
    require(book.tradeCount() == 1, "trade increments explicit noop count");
}

void testLobFillIsExplicitNoop() {
    LimitOrderBook book{42};

    book.apply(add(1, Side::Bid, P(100), 10));
    book.apply(fill(1, 5));

    require(book.volumeAt(Side::Bid, P(100)) == 10, "fill leaves bid volume unchanged");
    require(book.bestBid() == P(100), "fill leaves best bid unchanged");
    require(book.restingOrderCount() == 1, "fill leaves resting order count unchanged");
    require(book.fillCount() == 1, "fill increments explicit noop count");
}

} // namespace md::test
