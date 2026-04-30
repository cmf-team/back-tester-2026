#include "common/MarketDataEvent.hpp"
#include "catch2/catch_all.hpp"
#include <cstdint>

// ── priceToDouble
// ─────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent::priceToDouble", "[MarketDataEvent]")
{
    REQUIRE(MarketDataEvent::priceToDouble(0) == 0.0);

    // 1 unit = 1e-9 (fixed-point scale)
    REQUIRE(MarketDataEvent::priceToDouble(1'000'000'000LL) ==
            Catch::Approx(1.0));

    // From Databento docs: 5411750000000 → 5411.75
    REQUIRE(MarketDataEvent::priceToDouble(5'411'750'000'000LL) ==
            Catch::Approx(5411.75));

    // Negative prices are valid for calendar spreads (per Databento docs)
    REQUIRE(MarketDataEvent::priceToDouble(-1'000'000'000LL) ==
            Catch::Approx(-1.0));

    // Typical small options price: 0.02163
    REQUIRE(MarketDataEvent::priceToDouble(21'630'000LL) ==
            Catch::Approx(0.02163));
}