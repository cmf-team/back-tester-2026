#include "MarketDataReader.h"
#include <simdjson.h>

#include "common/LimitOrderBook.h"

void loadMarketData(
    const std::string& filepath,
    const std::function<void(const MarketDataEvent&, std::map<std::string, LimitOrderBook> &)>& consumer
    ) {
    simdjson::ondemand::parser parser;
    auto json = simdjson::padded_string::load(filepath).value();

    std::vector<MarketDataEvent> events;
    std::map<std::string, LimitOrderBook> orderbooks;
    auto stream = parser.iterate_many(json).value();
    for (auto doc : stream) {
        auto obj = doc.get_object().value();
        consumer(MarketDataEvent::fromJson(obj), orderbooks);
    }
}
