#include "common/MarketDataEvent.hpp"
#include "lob/LimitOrderBook.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cmf;

namespace {

MarketDataEvent makeEvent(std::uint32_t instrument_id, std::uint64_t order_id,
                          MdAction action, MdSide side, std::int64_t price,
                          std::uint32_t size, std::uint16_t publisher_id = 1) {
  MarketDataEvent event{};
  event.instrument_id = instrument_id;
  event.order_id = order_id;
  event.action = action;
  event.side = side;
  event.price = price;
  event.size = size;
  event.publisher_id = publisher_id;
  return event;
}

} // namespace

TEST_CASE("LimitOrderBook rebuilds aggregated bid and ask levels", "[lob]") {
  LimitOrderBook book(77);

  REQUIRE(book.apply(makeEvent(77, 11, MdAction::Add, MdSide::Bid,
                               1'100'000'000LL, 5))
              .missing_order == false);
  book.apply(makeEvent(77, 12, MdAction::Add, MdSide::Bid, 1'100'000'000LL, 7));
  book.apply(makeEvent(77, 21, MdAction::Add, MdSide::Ask, 1'200'000'000LL, 9));

  REQUIRE(book.volumeAtPrice(MdSide::Bid, 1'100'000'000LL) == 12);
  REQUIRE(book.volumeAtPrice(MdSide::Ask, 1'200'000'000LL) == 9);
  REQUIRE(book.bestBid()->price == 1'100'000'000LL);
  REQUIRE(book.bestBid()->size == 12);
  REQUIRE(book.bestAsk()->price == 1'200'000'000LL);
  REQUIRE(book.bestAsk()->size == 9);

  book.apply(makeEvent(77, 11, MdAction::Modify, MdSide::Bid, 1'150'000'000LL,
                       3));
  REQUIRE(book.volumeAtPrice(MdSide::Bid, 1'100'000'000LL) == 7);
  REQUIRE(book.volumeAtPrice(MdSide::Bid, 1'150'000'000LL) == 3);
  REQUIRE(book.bestBid()->price == 1'150'000'000LL);
  REQUIRE(book.bestBid()->size == 3);

  book.apply(makeEvent(77, 21, MdAction::Cancel, MdSide::Ask, UNDEF_PRICE, 4));
  REQUIRE(book.volumeAtPrice(MdSide::Ask, 1'200'000'000LL) == 5);

  book.apply(makeEvent(77, 0, MdAction::Trade, MdSide::None, 1'180'000'000LL,
                       2));
  book.apply(makeEvent(77, 21, MdAction::Fill, MdSide::Ask, 1'200'000'000LL,
                       2));
  REQUIRE(book.volumeAtPrice(MdSide::Ask, 1'200'000'000LL) == 5);

  book.apply(makeEvent(77, 0, MdAction::Clear, MdSide::None, UNDEF_PRICE, 0));
  REQUIRE_FALSE(book.bestBid().has_value());
  REQUIRE_FALSE(book.bestAsk().has_value());
  REQUIRE(book.orderCount() == 0);
}

TEST_CASE("LimitOrderBook reports missing orders and keeps prior fields on modify",
          "[lob]") {
  LimitOrderBook book(88);
  book.apply(makeEvent(88, 31, MdAction::Add, MdSide::Ask, 2'000'000'000LL, 6));

  MarketDataEvent keep_price =
      makeEvent(88, 31, MdAction::Modify, MdSide::None, UNDEF_PRICE, 4);
  const ApplyResult keep_price_result = book.apply(keep_price);
  REQUIRE_FALSE(keep_price_result.missing_order);
  REQUIRE(book.bestAsk()->price == 2'000'000'000LL);
  REQUIRE(book.bestAsk()->size == 4);

  const ApplyResult missing = book.apply(
      makeEvent(88, 99, MdAction::Cancel, MdSide::Ask, UNDEF_PRICE, 1));
  REQUIRE(missing.missing_order);
  REQUIRE(book.orderCount() == 1);
}
