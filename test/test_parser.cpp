#include <iostream>
#include <cmath>
#include "../src/common/JsonLineParser.h"

#define CHECK(cond, msg) \
    if (!(cond)) { std::cout << "FAIL: " << msg << "\n"; return 1; }

int main() {
    std::string line =
        R"({"ts_recv":"2026","action":"A","side":"B","price":"0.021200000","size":20,"order_id":"123"})";

    auto event = JsonLineParser::parse(line);

    CHECK(event.action == 'A', "action");
    CHECK(event.side == 'B', "side");
    CHECK(event.size == 20, "size");
    CHECK(event.order_id == "123", "order_id");
    CHECK(event.price.has_value(), "price exists");
    CHECK(std::fabs(event.price.value() - 0.0212) < 1e-6, "price value");

    std::cout << "OK\n";
    return 0;
}