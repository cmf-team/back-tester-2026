// Unit tests for the per-instrument Dispatcher: routing by instrument_id,
// order_id -> instrument_id resolution for cancel/trade without iid,
// modify-keeps-side, clear-resets-instrument.

#include "dispatcher/Dispatcher.hpp"
#include "common/BasicTypes.hpp"
#include "market_data/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

using namespace cmf;

namespace
{

constexpr std::int64_t kPrice = 1'000'000'000LL; // 1.0 in scaled units

MarketDataEvent makeEvent(MdAction action, OrderId order_id,
                          std::uint64_t iid, Side side, std::int64_t price,
                          std::uint32_t size)
{
    MarketDataEvent e;
    e.action = action;
    e.order_id = order_id;
    e.instrument_id = iid;
    e.side = side;
    e.price = price;
    e.size = size;
    return e;
}

} // namespace

TEST_CASE("Dispatcher - routes Add to per-instrument LOB", "[Dispatcher]")
{
    Dispatcher d;
    d.dispatch(makeEvent(MdAction::Add, 1, 100, Side::Buy, kPrice, 5));
    d.dispatch(makeEvent(MdAction::Add, 2, 200, Side::Sell, kPrice * 2, 7));
    const auto stats = d.finalize();
    REQUIRE(stats.events_routed == 2);
    REQUIRE(stats.instruments_touched == 2);
    REQUIRE(stats.orders_active == 2);
    REQUIRE(d.books().at(100).bbo().bid_size == 5);
    REQUIRE(d.books().at(200).bbo().ask_size == 7);
}

TEST_CASE("Dispatcher - resolves cancel without iid via order cache",
          "[Dispatcher]")
{
    Dispatcher d;
    d.dispatch(makeEvent(MdAction::Add, 42, 100, Side::Buy, kPrice, 5));
    // Cancel arrives with iid=0 — must be resolved from the order cache.
    d.dispatch(makeEvent(MdAction::Cancel, 42, 0, Side::None, 0, 5));
    const auto stats = d.finalize();
    REQUIRE(stats.events_routed == 2);
    REQUIRE(stats.unresolved_iid == 0);
    REQUIRE(d.books().at(100).bbo().bid_price.has_value() == false);
    REQUIRE(stats.orders_active == 0);
}

TEST_CASE("Dispatcher - drops cancel/trade without resolvable order",
          "[Dispatcher]")
{
    Dispatcher d;
    d.dispatch(makeEvent(MdAction::Cancel, 999, 0, Side::None, 0, 1));
    const auto stats = d.finalize();
    REQUIRE(stats.unresolved_iid == 1);
    REQUIRE(stats.events_routed == 0);
}

TEST_CASE("Dispatcher - partial cancel keeps order alive", "[Dispatcher]")
{
    Dispatcher d;
    d.dispatch(makeEvent(MdAction::Add, 7, 100, Side::Buy, kPrice, 10));
    d.dispatch(makeEvent(MdAction::Cancel, 7, 0, Side::None, 0, 4));
    const auto stats = d.finalize();
    REQUIRE(stats.orders_active == 1);
    REQUIRE(d.books().at(100).bbo().bid_size == 6);
}

TEST_CASE("Dispatcher - Modify moves order to a new level", "[Dispatcher]")
{
    Dispatcher d;
    d.dispatch(makeEvent(MdAction::Add, 1, 100, Side::Buy, kPrice, 5));
    // Modify: same side, new price=2.0, new qty=8
    MarketDataEvent mod = makeEvent(MdAction::Modify, 1, 0, Side::None,
                                    2 * kPrice, 8);
    d.dispatch(mod);
    d.finalize();
    const auto& book = d.books().at(100);
    REQUIRE(book.volumeAtPrice(Side::Buy, kPrice) == 0);
    REQUIRE(book.volumeAtPrice(Side::Buy, 2 * kPrice) == 8);
}

TEST_CASE("Dispatcher - Trade with full size erases the order", "[Dispatcher]")
{
    Dispatcher d;
    d.dispatch(makeEvent(MdAction::Add, 1, 100, Side::Sell, kPrice, 5));
    d.dispatch(makeEvent(MdAction::Trade, 1, 0, Side::None, 0, 5));
    const auto stats = d.finalize();
    REQUIRE(stats.orders_active == 0);
    REQUIRE(d.books().at(100).askLevels() == 0);
}

TEST_CASE("Dispatcher - Clear resets the instrument", "[Dispatcher]")
{
    Dispatcher d;
    d.dispatch(makeEvent(MdAction::Add, 1, 100, Side::Buy, kPrice, 5));
    d.dispatch(makeEvent(MdAction::Add, 2, 100, Side::Sell, 2 * kPrice, 5));
    // Add to a different instrument that must NOT be cleared.
    d.dispatch(makeEvent(MdAction::Add, 3, 200, Side::Buy, kPrice, 1));
    MarketDataEvent clr;
    clr.action = MdAction::Clear;
    clr.instrument_id = 100;
    d.dispatch(clr);
    d.finalize();
    REQUIRE(d.books().at(100).bidLevels() == 0);
    REQUIRE(d.books().at(100).askLevels() == 0);
    REQUIRE(d.books().at(200).bidLevels() == 1);
}
