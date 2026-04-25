// Unit tests for TimestampParser: Databento pretty_ts ISO 8601 -> NanoTime.

#include "market_data/TimestampParser.hpp"

#include "catch2/catch_all.hpp"

#include <stdexcept>
#include <string>

using namespace cmf;

TEST_CASE("TimestampParser - epoch", "[TimestampParser]")
{
    REQUIRE(parseDatabentoTimestamp("1970-01-01T00:00:00.000000000Z") == 0);
}

TEST_CASE("TimestampParser - real Databento sample from 2026-04-06",
          "[TimestampParser]")
{
    // Ground truth computed independently:
    //   datetime(2026,4,6,18,53,8,tzinfo=timezone.utc).timestamp()*1e9 +
    //   486368500 = 1775501588486368500
    REQUIRE(parseDatabentoTimestamp("2026-04-06T18:53:08.486368500Z") ==
            1775501588486368500LL);
}

TEST_CASE("TimestampParser - leap-year Feb 29 -> Mar 1 is 86400 s apart",
          "[TimestampParser]")
{
    const auto a = parseDatabentoTimestamp("2024-02-29T00:00:00.000000000Z");
    const auto b = parseDatabentoTimestamp("2024-03-01T00:00:00.000000000Z");
    REQUIRE(b - a == 86'400'000'000'000LL);
}

TEST_CASE("TimestampParser - nanosecond resolution", "[TimestampParser]")
{
    const auto a = parseDatabentoTimestamp("2026-04-06T18:53:08.000000000Z");
    const auto b = parseDatabentoTimestamp("2026-04-06T18:53:08.000000001Z");
    REQUIRE(b - a == 1);
}

TEST_CASE("TimestampParser - monotonic across day boundary",
          "[TimestampParser]")
{
    const auto a = parseDatabentoTimestamp("2026-04-06T23:59:59.999999999Z");
    const auto b = parseDatabentoTimestamp("2026-04-07T00:00:00.000000000Z");
    REQUIRE(b == a + 1);
}

TEST_CASE("TimestampParser - malformed input rejected", "[TimestampParser]")
{
    REQUIRE_THROWS_AS(parseDatabentoTimestamp(""), std::invalid_argument);
    REQUIRE_THROWS_AS(parseDatabentoTimestamp("not a timestamp"),
                      std::invalid_argument);
    // Wrong length (missing trailing Z):
    REQUIRE_THROWS_AS(parseDatabentoTimestamp("2026-04-06T18:53:08.486368500"),
                      std::invalid_argument);
    // Missing T separator:
    REQUIRE_THROWS_AS(parseDatabentoTimestamp("2026-04-06 18:53:08.486368500Z"),
                      std::invalid_argument);
    // Non-digit in field:
    REQUIRE_THROWS_AS(parseDatabentoTimestamp("2026-04-06T1X:53:08.486368500Z"),
                      std::invalid_argument);
    // Out-of-range month:
    REQUIRE_THROWS_AS(parseDatabentoTimestamp("2026-13-06T18:53:08.486368500Z"),
                      std::invalid_argument);
    // Out-of-range hour:
    REQUIRE_THROWS_AS(parseDatabentoTimestamp("2026-04-06T24:00:00.000000000Z"),
                      std::invalid_argument);
}
