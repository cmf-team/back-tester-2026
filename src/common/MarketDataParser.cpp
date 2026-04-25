#include "MarketDataParser.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <string>

#include <chrono>
#include <sstream>

using json = nlohmann::json;

// Helper to convert ISO 8601 string to nanoseconds since epoch
uint64_t isoToNanos(const std::string& iso_str)
{
    if (iso_str.empty())
        return MarketDataEvent::UNDEF_TIMESTAMP;
    std::tm tm = {};
    std::istringstream ss(iso_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    // Extract fractional part manually
    auto dot = iso_str.find('.');
    auto z = iso_str.find('Z');
    uint64_t nanos = 0;
    if (dot != std::string::npos && z != std::string::npos)
    {
        std::string frac = iso_str.substr(dot + 1, z - dot - 1);
        while (frac.size() < 9)
            frac += '0'; // Pad to nanos
        nanos = std::stoull(frac.substr(0, 9));
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               tp.time_since_epoch())
               .count() +
           nanos;
}

Action actionFromString(const std::string& str)
{
    if (str == "A")
        return Action::Add;
    else if (str == "M")
        return Action::Modify;
    else if (str == "C")
        return Action::Cancel;
    else if (str == "R")
        return Action::Clear;
    else if (str == "T")
        return Action::Trade;
    else if (str == "F")
        return Action::Fill;
    else
        return Action::None;
}

Side sideFromString(const std::string& str)
{
    if (str == "B")
        return Side::Bid;
    else if (str == "A")
        return Side::Ask;
    else
        return Side::None;
}

std::optional<MarketDataEvent> parseNDJSON(const std::string& line)
{
    try
    {
        MarketDataEvent event;
        auto j = json::parse(line);
        auto hd = j.at("hd");

        // Databento Rule: sort_ts = ts_recv (if exists) else ts_event
        event.ts_recv = isoToNanos(j.value("ts_recv", ""));
        event.ts_event = isoToNanos(hd.value("ts_event", ""));
        event.sort_ts = (event.ts_recv != MarketDataEvent::UNDEF_TIMESTAMP)
                            ? event.ts_recv
                            : event.ts_event;
        event.rtype = hd.value("rtype", RType::MBP_0);
        event.publisher_id = hd.value("publisher_id", 0u);
        event.instrument_id = hd.value("instrument_id", 0u);
        event.action = actionFromString(j.at("action").get<std::string>());
        event.side = sideFromString(j.at("side").get<std::string>());
        event.price =
            static_cast<int64_t>(std::stod(j.at("price").get<std::string>()) *
                                 1e9); // Decimal to fixed-point
        event.size = j.value("size", 0u);
        event.channel_id = j.value("channel_id", 0u);
        event.order_id = std::stoull(
            j.at("order_id").get<std::string>()); // Handle "order_id" as string
        event.flag = j.value("flags", Flag::None);
        event.ts_in_delta = j.value("ts_in_delta", 0);

        return event;
    }
    catch (...)
    {
        return std::nullopt;
    }
}