#include "MarketDataReader.h"
#include <simdjson.h>

void loadMarketData(
    const std::string& filepath,
    const std::function<void(const MarketDataEvent&, MarketDataEventSumary&)>& consumer
    ) {
    simdjson::ondemand::parser parser;
    auto json = simdjson::padded_string::load(filepath).value();

    std::vector<MarketDataEvent> events;
    MarketDataEventSumary summary;
    auto stream = parser.iterate_many(json).value();
    for (auto doc : stream) {
        auto obj = doc.get_object().value();
        consumer(MarketDataEvent::fromJson(obj), summary);
    }
    std::cout << "Summary:\n" << std::endl
              << " | First timestamp: " << summary.first_timestamp << " |" << std::endl
              << " | Last timestamp: " << summary.last_timestamp << " |" << std::endl
              << " | Total count: " << summary.total_message_count << " |" << std::endl
              << " | First events: " << std::endl;
    summary.printFirstEvents();
    std::cout << " | Last events: " << std::endl;
    summary.printLastEvents();
}