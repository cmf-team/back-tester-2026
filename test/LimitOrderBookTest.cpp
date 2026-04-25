// Unit tests for the L2 LimitOrderBook (signed-delta API).

#include "order_book/LimitOrderBook.hpp"
#include "common/BasicTypes.hpp"
#include "market_data/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

#include <sstream>
#include <stdexcept>

using namespace cmf;

namespace
{
constexpr std::int64_t px(double v)
{
    return static_cast<std::int64_t>(v * MarketDataEvent::kPriceScale);
}
} // namespace

TEST_CASE("LOB - empty book has no BBO", "[LOB]")
{
    LimitOrderBook lob;
    const auto b = lob.bbo();
    REQUIRE_FALSE(b.bid_price.has_value());
    REQUIRE_FALSE(b.ask_price.has_value());
    REQUIRE(lob.bidLevels() == 0);
    REQUIRE(lob.askLevels() == 0);
}

TEST_CASE("LOB - single Add creates a level", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 5);
    const auto b = lob.bbo();
    REQUIRE(b.bid_price.has_value());
    REQUIRE(*b.bid_price == px(1.0));
    REQUIRE(b.bid_size == 5);
}

TEST_CASE("LOB - aggregates multiple orders at same level", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 5);
    lob.applyDelta(Side::Buy, px(1.0), 3);
    REQUIRE(lob.volumeAtPrice(Side::Buy, px(1.0)) == 8);
}

TEST_CASE("LOB - best bid is the highest, best ask is the lowest", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 1);
    lob.applyDelta(Side::Buy, px(1.5), 2);
    lob.applyDelta(Side::Buy, px(0.5), 3);
    lob.applyDelta(Side::Sell, px(2.0), 4);
    lob.applyDelta(Side::Sell, px(2.5), 5);
    lob.applyDelta(Side::Sell, px(2.2), 6);

    const auto b = lob.bbo();
    REQUIRE(*b.bid_price == px(1.5));
    REQUIRE(b.bid_size == 2);
    REQUIRE(*b.ask_price == px(2.0));
    REQUIRE(b.ask_size == 4);
}

TEST_CASE("LOB - cancel partial reduces level", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 5);
    lob.applyDelta(Side::Buy, px(1.0), -2);
    REQUIRE(lob.volumeAtPrice(Side::Buy, px(1.0)) == 3);
}

TEST_CASE("LOB - cancel full erases level", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 5);
    lob.applyDelta(Side::Buy, px(1.0), -5);
    REQUIRE(lob.volumeAtPrice(Side::Buy, px(1.0)) == 0);
    REQUIRE(lob.bidLevels() == 0);
}

TEST_CASE("LOB - over-cancel still erases level (defensive)", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 3);
    lob.applyDelta(Side::Buy, px(1.0), -10);
    REQUIRE(lob.bidLevels() == 0);
    REQUIRE(lob.volumeAtPrice(Side::Buy, px(1.0)) == 0);
}

TEST_CASE("LOB - clear empties both sides", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 5);
    lob.applyDelta(Side::Sell, px(2.0), 4);
    lob.clear();
    REQUIRE(lob.bidLevels() == 0);
    REQUIRE(lob.askLevels() == 0);
    REQUIRE_FALSE(lob.bbo().bid_price.has_value());
}

TEST_CASE("LOB - delta of zero is a no-op", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, px(1.0), 5);
    lob.applyDelta(Side::Buy, px(1.0), 0);
    REQUIRE(lob.volumeAtPrice(Side::Buy, px(1.0)) == 5);
}

TEST_CASE("LOB - undef price is ignored", "[LOB]")
{
    LimitOrderBook lob;
    lob.applyDelta(Side::Buy, MarketDataEvent::kUndefPrice, 5);
    REQUIRE(lob.bidLevels() == 0);
}

TEST_CASE("LOB - Side::None throws", "[LOB]")
{
    LimitOrderBook lob;
    REQUIRE_THROWS_AS(lob.applyDelta(Side::None, px(1.0), 5),
                      std::invalid_argument);
}

TEST_CASE("LOB - volumeAtPrice returns 0 on missing level", "[LOB]")
{
    LimitOrderBook lob;
    REQUIRE(lob.volumeAtPrice(Side::Buy, px(1.0)) == 0);
    REQUIRE(lob.volumeAtPrice(Side::Sell, px(2.0)) == 0);
}

TEST_CASE("LOB - printSnapshot respects depth and sides", "[LOB]")
{
    LimitOrderBook lob;
    for (int i = 1; i <= 5; ++i)
    {
        lob.applyDelta(Side::Buy, px(static_cast<double>(i)), i);
        lob.applyDelta(Side::Sell, px(10.0 + i), 10 + i);
    }
    std::ostringstream os;
    lob.printSnapshot(os, 2);
    const auto s = os.str();
    REQUIRE(s.find("bid[L1]") != std::string::npos);
    REQUIRE(s.find("bid[L2]") != std::string::npos);
    REQUIRE(s.find("bid[L3]") == std::string::npos);
    REQUIRE(s.find("ask[L1]") != std::string::npos);
    REQUIRE(s.find("ask[L2]") != std::string::npos);
    REQUIRE(s.find("ask[L3]") == std::string::npos);
}
