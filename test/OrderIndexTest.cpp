#include "lob/OrderIndex.hpp"

#include "catch2/catch_all.hpp"

using namespace cmf;

TEST_CASE("OrderIndex - insert/find/erase round-trip", "[order_index]") {
    OrderIndex idx(16);
    OrderRecord rec{7, 'B', 1234, 5};
    idx.insert(42, rec);

    OrderRecord out;
    REQUIRE(idx.find(42, out));
    REQUIRE(out.instrument_id == 7u);
    REQUIRE(out.side == 'B');
    REQUIRE(out.scaled_price == 1234);
    REQUIRE(out.remaining_qty == 5u);
    REQUIRE(idx.size() == 1u);

    idx.erase(42);
    REQUIRE_FALSE(idx.find(42, out));
    REQUIRE(idx.size() == 0u);
}

TEST_CASE("OrderIndex - update_qty changes only qty", "[order_index]") {
    OrderIndex idx;
    idx.insert(1, {3, 'A', 555, 9});
    REQUIRE(idx.update_qty(1, 2));
    OrderRecord out;
    REQUIRE(idx.find(1, out));
    REQUIRE(out.remaining_qty == 2u);
    REQUIRE(out.scaled_price == 555);
}

TEST_CASE("OrderIndex - update_qty on missing returns false", "[order_index]") {
    OrderIndex idx;
    REQUIRE_FALSE(idx.update_qty(999, 1));
}
