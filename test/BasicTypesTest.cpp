// tests for BasicTypes

#include "common/BasicTypes.hpp"

#include "catch2/catch_all.hpp"

using namespace cmf;

TEST_CASE("BasicTypes - Side", "[BasicTypes]")
{
    REQUIRE(int(Side::Buy)  == 1);
    REQUIRE(int(Side::Sell) == -1);
    REQUIRE(int(Side::None) == 0);
}
