// Tests for Lob (single-instrument, tick-bucket bitmap book).

#include "lob/Lob.hpp"
#include "parser/MarketDataEvent.hpp"

#include "catch2/catch_all.hpp"

#include <sstream>

using namespace cmf;

namespace {

// 0.01 tick in 1e-9 fixed point — matches the default LobConfig.
constexpr int64_t kTick   = 10'000'000LL;
// Anchor at $100.00 = 100 * 1e9.
constexpr int64_t kAnchor = 100LL * MarketDataEvent::kPriceScale;

}  // namespace

TEST_CASE("Lob add -> best bid price + volume", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 10);
  REQUIRE(lob.hasBid());
  REQUIRE_FALSE(lob.hasAsk());
  REQUIRE(lob.bestBidPrice()  == kAnchor);
  REQUIRE(lob.bestBidVolume() == 10u);
  REQUIRE(lob.bidLevels() == 1u);
  REQUIRE(lob.anchored());
}

TEST_CASE("Lob better bid wins regardless of insertion order", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor,          10);
  lob.add(2, MdSide::Bid, kAnchor + kTick,   5);
  REQUIRE(lob.bestBidPrice()  == kAnchor + kTick);
  REQUIRE(lob.bestBidVolume() == 5u);
}

TEST_CASE("Lob lower ask wins", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Ask, kAnchor + 2 * kTick, 10);
  lob.add(2, MdSide::Ask, kAnchor + 1 * kTick,  3);
  REQUIRE(lob.bestAskPrice()  == kAnchor + 1 * kTick);
  REQUIRE(lob.bestAskVolume() == 3u);
}

TEST_CASE("Lob full cancel removes level", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 10);
  lob.cancel(1, 10);
  REQUIRE_FALSE(lob.hasBid());
  REQUIRE(lob.bidLevels() == 0u);
  REQUIRE(lob.orderCount() == 0u);
}

TEST_CASE("Lob partial cancel preserves level, reduces volume", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 10);
  lob.cancel(1, 3);
  REQUIRE(lob.hasBid());
  REQUIRE(lob.bestBidVolume() == 7u);
  REQUIRE(lob.bidLevels() == 1u);
  REQUIRE(lob.orderCount() == 1u);
}

TEST_CASE("Lob cancel underflow clamps to full + logs once per instrument",
          "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 5);
  lob.add(2, MdSide::Bid, kAnchor, 5);
  lob.cancel(1, 999);  // over-cancel
  lob.cancel(2, 999);  // over-cancel again — still counted, not re-logged
  REQUIRE(lob.underflowWarnings() == 2u);
  REQUIRE_FALSE(lob.hasBid());
  REQUIRE(lob.orderCount() == 0u);
}

TEST_CASE("Lob cancel for unknown order logs once, no-op", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 5);
  lob.cancel(999, 1);
  REQUIRE(lob.missingOrderWarnings() == 1u);
  REQUIRE(lob.bestBidVolume() == 5u);
}

TEST_CASE("Lob modify at same bucket adjusts size only", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 10);
  lob.modify(1, MdSide::Bid, kAnchor, 3);
  REQUIRE(lob.bestBidVolume() == 3u);
  REQUIRE(lob.bidLevels() == 1u);
  lob.modify(1, MdSide::Bid, kAnchor, 20);
  REQUIRE(lob.bestBidVolume() == 20u);
}

TEST_CASE("Lob modify to new price moves bucket, vacates old", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor,           10);
  lob.modify(1, MdSide::Bid, kAnchor + 5 * kTick, 7);
  REQUIRE(lob.bestBidPrice()  == kAnchor + 5 * kTick);
  REQUIRE(lob.bestBidVolume() == 7u);
  REQUIRE(lob.bidLevels() == 1u);
}

TEST_CASE("Lob clear wipes book, preserves anchor", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor,         10);
  lob.add(2, MdSide::Ask, kAnchor + kTick,  5);
  const int64_t floor_before = lob.priceFloor();
  lob.clear();
  REQUIRE_FALSE(lob.hasBid());
  REQUIRE_FALSE(lob.hasAsk());
  REQUIRE(lob.orderCount() == 0u);
  REQUIRE(lob.anchored());
  REQUIRE(lob.priceFloor() == floor_before);
}

TEST_CASE("Lob aggregates multiple orders at same bucket", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 10);
  lob.add(2, MdSide::Bid, kAnchor,  4);
  REQUIRE(lob.bestBidVolume() == 14u);
  REQUIRE(lob.bidLevels() == 1u);
  lob.cancel(1, 10);
  REQUIRE(lob.hasBid());
  REQUIRE(lob.bestBidVolume() == 4u);
}

TEST_CASE("Lob volumeAt 0 outside populated levels", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 10);
  REQUIRE(lob.volumeAt(MdSide::Bid, kAnchor)          == 10u);
  REQUIRE(lob.volumeAt(MdSide::Bid, kAnchor + kTick)  == 0u);
  REQUIRE(lob.volumeAt(MdSide::Ask, kAnchor)          == 0u);
}

TEST_CASE("Lob out-of-range price triggers oor warning, no mutation",
          "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor, 10);  // anchors around $100
  // 10_000 ticks = $100 above anchor — well outside the 4096-tick window.
  lob.add(2, MdSide::Bid, kAnchor + 10'000 * kTick, 5);
  REQUIRE(lob.oorWarnings() == 1u);
  REQUIRE(lob.bidLevels() == 1u);
  REQUIRE(lob.bestBidVolume() == 10u);
}

TEST_CASE("Lob best bid/ask correct across level-0 word boundaries", "[Lob]") {
  Lob lob{42};
  // Anchor centers at bucket 2048. Populate offsets that span distinct
  // level-0 words (each word covers 64 buckets).
  lob.add(1, MdSide::Bid, kAnchor - 1000 * kTick, 1);
  lob.add(2, MdSide::Bid, kAnchor -   64 * kTick, 2);
  lob.add(3, MdSide::Bid, kAnchor -        kTick, 3);
  REQUIRE(lob.bestBidPrice()  == kAnchor - kTick);
  REQUIRE(lob.bestBidVolume() == 3u);

  lob.add(4, MdSide::Ask, kAnchor +        kTick, 4);
  lob.add(5, MdSide::Ask, kAnchor +   64 * kTick, 5);
  lob.add(6, MdSide::Ask, kAnchor + 1000 * kTick, 6);
  REQUIRE(lob.bestAskPrice()  == kAnchor + kTick);
  REQUIRE(lob.bestAskVolume() == 4u);

  // Peel bids: each cancel should reveal the next-best across word/summary.
  lob.cancel(3, 3);
  REQUIRE(lob.bestBidPrice() == kAnchor - 64 * kTick);
  lob.cancel(2, 2);
  REQUIRE(lob.bestBidPrice() == kAnchor - 1000 * kTick);
  lob.cancel(1, 1);
  REQUIRE_FALSE(lob.hasBid());

  // Peel asks.
  lob.cancel(4, 4);
  REQUIRE(lob.bestAskPrice() == kAnchor + 64 * kTick);
  lob.cancel(5, 5);
  REQUIRE(lob.bestAskPrice() == kAnchor + 1000 * kTick);
  lob.cancel(6, 6);
  REQUIRE_FALSE(lob.hasAsk());
}

TEST_CASE("Lob snapshot prints levels in correct order", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::Bid, kAnchor - 1 * kTick,  5);
  lob.add(2, MdSide::Bid, kAnchor - 2 * kTick, 10);
  lob.add(3, MdSide::Ask, kAnchor + 1 * kTick,  3);
  lob.add(4, MdSide::Ask, kAnchor + 2 * kTick,  7);

  std::ostringstream os;
  lob.printSnapshot(os, 5);
  const auto s = os.str();

  // Best bid ($99.99) printed before worse bid ($99.98).
  const auto pos_bid_best  = s.find("99.990000000");
  const auto pos_bid_worst = s.find("99.980000000");
  REQUIRE(pos_bid_best  != std::string::npos);
  REQUIRE(pos_bid_worst != std::string::npos);
  REQUIRE(pos_bid_best < pos_bid_worst);

  // Best ask ($100.01) printed before worse ask ($100.02).
  const auto pos_ask_best  = s.find("100.010000000");
  const auto pos_ask_worst = s.find("100.020000000");
  REQUIRE(pos_ask_best  != std::string::npos);
  REQUIRE(pos_ask_worst != std::string::npos);
  REQUIRE(pos_ask_best < pos_ask_worst);
}

TEST_CASE("Lob auto-detects tick from first price by default", "[Lob]") {
  Lob lob{42};
  // 1.156100000 = 1'156'100'000 fixed-point.
  // Largest pow-10 divisor = 100'000 (well under the $0.01 cap).
  lob.add(1, MdSide::Bid, 1'156'100'000LL, 10);
  REQUIRE(lob.config().tick_size == 100'000LL);
  REQUIRE(lob.bestBidPrice()  == 1'156'100'000LL);
  REQUIRE(lob.bestBidVolume() == 10u);
}

TEST_CASE("Lob auto-detect caps round prices at auto_tick_max", "[Lob]") {
  Lob lob{42};
  // $100.00 = 1e11 — divisible by 1e11. Cap kicks in at $0.01.
  lob.add(1, MdSide::Bid, 100LL * MarketDataEvent::kPriceScale, 10);
  REQUIRE(lob.config().tick_size == 10'000'000LL);
}

TEST_CASE("Lob clamps floor to 0 when centered window would go negative",
          "[Lob]") {
  Lob lob{42};
  // First price 7'730'000 (= 0.00773). Auto-detected tick = 10'000.
  // Centered floor would be 7'730'000 - 2048*10'000 = -12'750'000.
  // Clamp -> floor = 0, window [0, 40'960'000).
  lob.add(1, MdSide::Bid, 7'730'000LL, 10);
  REQUIRE(lob.config().tick_size == 10'000LL);
  REQUIRE(lob.priceFloor() == 0);
  REQUIRE(lob.bestBidPrice()  == 7'730'000LL);
  REQUIRE(lob.bestBidVolume() == 10u);
  // The price is no longer at the center bucket — it's at 7'730'000 / 10'000 = 773.
  // A price near the upper end of the window must still resolve correctly.
  lob.add(2, MdSide::Ask, 28'200'000LL, 4);
  REQUIRE(lob.bestAskPrice() == 28'200'000LL);
}

TEST_CASE("Lob does not clamp for negative first price", "[Lob]") {
  LobConfig cfg;
  cfg.tick_size = 10'000LL;  // explicit; auto-detect would fail on a 1-divisor
  Lob lob{42, cfg};
  // Negative-price calendar spread. Window stays centered on the first price.
  lob.add(1, MdSide::Bid, -1'000'000LL, 10);
  REQUIRE(lob.priceFloor() < 0);
  REQUIRE(lob.bestBidPrice() == -1'000'000LL);
}

TEST_CASE("Lob explicit tick_size disables auto-detect", "[Lob]") {
  LobConfig cfg;
  cfg.tick_size = 50'000'000LL;  // $0.05
  Lob lob{42, cfg};
  lob.add(1, MdSide::Bid, 100LL * MarketDataEvent::kPriceScale, 10);
  REQUIRE(lob.config().tick_size == 50'000'000LL);
}

TEST_CASE("Lob side=None on mutation is a no-op", "[Lob]") {
  Lob lob{42};
  lob.add(1, MdSide::None, kAnchor, 10);
  REQUIRE_FALSE(lob.hasBid());
  REQUIRE_FALSE(lob.hasAsk());
  REQUIRE(lob.orderCount() == 0u);
}
