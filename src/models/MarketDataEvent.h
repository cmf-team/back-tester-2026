#pragma once
#include <string>
#include <simdjson.h>
#include <queue>

struct MarketDataEvent {
    std::string symbol;
    int instrument_id;
    std::string ts_event;
    double price;
    double size;
    std::string side;
    std::string action;
    int channel_id;
    std::string order_id;
    int flags;

    friend std::ostream& operator<<(std::ostream& os, const MarketDataEvent& event) {
        os << "symbol=" << event.symbol
           << " price=" << event.price
           << " size="  << event.size
           << " side="  << event.side
           << " action="<< event.action;
        return os;
    }

    static MarketDataEvent fromJson(simdjson::ondemand::object& obj) {
        MarketDataEvent event;

        // простые поля
        event.symbol   = std::string(obj["symbol"].get_string().value());
        event.side     = std::string(obj["side"].get_string().value());
        event.action   = std::string(obj["action"].get_string().value());
        event.order_id = std::string(obj["order_id"].get_string().value());
        event.size     = obj["size"].get_double().value();
        event.channel_id = static_cast<int>(obj["channel_id"].get_int64().value());
        event.flags      = static_cast<int>(obj["flags"].get_int64().value());

        auto price_val = obj["price"];
        if (price_val.type() == simdjson::ondemand::json_type::null) {
            event.price = 0.0;
        } else {
            std::string_view price_str = price_val.get_string().value();
            event.price = std::stod(std::string(price_str));
        }

        auto hd = obj["hd"].get_object().value();
        event.instrument_id = static_cast<int>(hd["instrument_id"].get_int64().value());
        event.ts_event      = std::string(hd["ts_event"].get_string().value());

        return event;
    }
};


struct MarketDataEventSumary {
    int total_message_count = 0;
    std::string first_timestamp;
    std::string last_timestamp;
    std::deque<MarketDataEvent> firstEvents;
    std::deque<MarketDataEvent> lastEvents;

    void printFirstEvents() const {
        for (const auto& event : firstEvents) {
            std::cout << event << "\n";
        }
    }

    void printLastEvents() const {
        for (const auto& event : lastEvents) {
            std::cout << event << "\n";
        }
    }
};
