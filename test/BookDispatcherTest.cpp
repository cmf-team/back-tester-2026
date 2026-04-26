#include "common/MarketDataEvent.hpp"
#include "lob/BookDispatcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace cmf;

namespace {

MarketDataEvent makeEvent(std::uint32_t instrument_id, std::uint64_t order_id,
                          MdAction action, MdSide side, std::int64_t price,
                          std::uint32_t size, std::uint16_t publisher_id = 1,
                          std::uint64_t ts_recv = 0) {
  MarketDataEvent event{};
  event.instrument_id = instrument_id;
  event.order_id = order_id;
  event.action = action;
  event.side = side;
  event.price = price;
  event.size = size;
  event.publisher_id = publisher_id;
  event.ts_recv = ts_recv;
  return event;
}

} // namespace

TEST_CASE("BookDispatcher routes explicit instrument events and captures snapshots",
          "[lob][dispatcher]") {
  BookDispatcher dispatcher(
      DispatchOptions{.snapshot_every = 2, .max_snapshots = 3, .snapshot_depth = 2});

  dispatcher.apply(
      makeEvent(10, 101, MdAction::Add, MdSide::Bid, 1'000'000'000LL, 5, 1, 1));
  dispatcher.apply(
      makeEvent(11, 201, MdAction::Add, MdSide::Ask, 2'000'000'000LL, 7, 1, 2));
  dispatcher.apply(makeEvent(10, 101, MdAction::Cancel, MdSide::Bid, UNDEF_PRICE,
                             2, 1, 3));
  dispatcher.apply(
      makeEvent(11, 202, MdAction::Add, MdSide::Ask, 2'100'000'000LL, 3, 1, 4));

  REQUIRE(dispatcher.stats().total_events == 4);
  REQUIRE(dispatcher.stats().instruments == 2);
  REQUIRE(dispatcher.stats().snapshots == 3);

  const auto summaries = dispatcher.finalSummaries();
  REQUIRE(summaries.size() == 2);
  const auto find_summary = [&](std::uint32_t instrument_id) -> const BookSummary & {
    const auto it =
        std::find_if(summaries.begin(), summaries.end(),
                     [&](const BookSummary &summary) {
                       return summary.instrument_id == instrument_id;
                     });
    REQUIRE(it != summaries.end());
    return *it;
  };
  const BookSummary &book10 = find_summary(10);
  const BookSummary &book11 = find_summary(11);
  REQUIRE(book10.orders == 1);
  REQUIRE(book10.best_bid->size == 3);
  REQUIRE(book11.best_ask->price == 2'000'000'000LL);
  REQUIRE(book11.snapshot.find("best_ask=2.000000000x7") != std::string::npos);
}

TEST_CASE("BookDispatcher resolves missing instrument ids when order is unique",
          "[lob][dispatcher]") {
  BookDispatcher dispatcher;
  dispatcher.apply(
      makeEvent(42, 700, MdAction::Add, MdSide::Bid, 5'000'000'000LL, 4, 9, 1));

  MarketDataEvent cancel_without_instrument =
      makeEvent(0, 700, MdAction::Cancel, MdSide::Bid, UNDEF_PRICE, 4, 9, 2);
  dispatcher.apply(cancel_without_instrument);

  REQUIRE(dispatcher.stats().unresolved_routes == 0);
  REQUIRE(dispatcher.stats().ambiguous_routes == 0);
  REQUIRE(dispatcher.finalSummaries()[0].orders == 0);
}

TEST_CASE("BookDispatcher flags ambiguous missing-instrument routing",
          "[lob][dispatcher]") {
  BookDispatcher dispatcher;
  dispatcher.apply(
      makeEvent(42, 700, MdAction::Add, MdSide::Bid, 5'000'000'000LL, 4, 9, 1));
  dispatcher.apply(
      makeEvent(43, 700, MdAction::Add, MdSide::Ask, 6'000'000'000LL, 2, 9, 2));

  MarketDataEvent cancel_without_instrument =
      makeEvent(0, 700, MdAction::Cancel, MdSide::Bid, UNDEF_PRICE, 1, 9, 3);
  dispatcher.apply(cancel_without_instrument);

  REQUIRE(dispatcher.stats().ambiguous_routes == 1);
  const auto summaries = dispatcher.finalSummaries();
  REQUIRE(summaries.size() == 2);
  REQUIRE(summaries[0].orders == 1);
  REQUIRE(summaries[1].orders == 1);
}
