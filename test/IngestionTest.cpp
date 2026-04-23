#include "ingestion/JsonParser.hpp"
#include "catch2/catch_all.hpp"

using namespace cmf;

TEST_CASE("Parse simple JSON line") {
    std::string line = R"({"timestamp":1,"order_id":42,"side":"buy","price":1.5,"size":10,"action":"add"})";

    auto evt = parseLine(line);

    REQUIRE(evt.has_value());
    REQUIRE(evt->orderId == 42);
    REQUIRE(evt->price == 1.5);
}