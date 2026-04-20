#include "common/MarketDataEvent.hpp"
#include "data_layer/JsonParser.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace cmf;

int main() {
    // Тест 1: валидная строка с price=null
    std::string_view valid = R"({"ts_recv":"2026-04-06T18:53:11.430340287Z","ts_event":"2026-04-06T18:53:11.430338519Z","rtype":160,"publisher_id":101,"instrument_id":33974,"action":"R","side":"N","price":null,"size":0,"channel_id":79,"order_id":"0","flags":128,"ts_in_delta":1768,"sequence":0,"symbol":"TEST"})";

    auto ev = parse_mbo_line(valid);
    assert(ev.has_value());
    assert(ev->rtype == 160);
    assert(ev->action == 'R');
    assert(std::isnan(ev->price));
    assert(strcmp(ev->symbol, "TEST") == 0);

    // Тест 2: невалидная строка
    auto invalid = parse_mbo_line("not json");
    assert(!invalid.has_value());

    // Тест 3: цена как число
    std::string_view with_price = R"({"ts_recv":"2026-04-06T18:53:11.430340287Z","ts_event":"2026-04-06T18:53:11.430338519Z","rtype":160,"publisher_id":101,"instrument_id":33974,"action":"A","side":"B","price":150.25,"size":100,"channel_id":79,"order_id":"123","flags":0,"ts_in_delta":0,"sequence":1,"symbol":"AAPL"})";
    auto ev2 = parse_mbo_line(with_price);
    assert(ev2.has_value());
    assert(ev2->price == 150.25);
    assert(ev2->size == 100);

    std::cout << "All parser tests passed!\n";
    return 0;
}