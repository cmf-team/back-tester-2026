#include "MarketDataEvent.hpp"

MarketDataEvent MarketDataEvent::fromJson(const json& j)
{
    MarketDataEvent e;

    e.tsRecv = j.value("ts_recv", "");

    if (j.contains("hd"))
    {
        const auto& hd = j["hd"];

        e.tsEvent = hd.value("ts_event", "");
        e.rtype = hd.value("rtype", 0);
        e.publisherId = hd.value("publisher_id", 0);
        e.instrumentId = hd.value("instrument_id", 0LL);
    }

    std::string actionValue = j.value("action", "N");
    std::string sideValue = j.value("side", "N");

    e.action = actionValue.empty() ? 'N' : actionValue[0];
    e.side = sideValue.empty() ? 'N' : sideValue[0];

    if (j.contains("price") && !j["price"].is_null())
    {
        if (j["price"].is_string())
            e.price = j["price"].get<std::string>();
        else
            e.price = std::to_string(j["price"].get<double>());
    }

    e.size = j.value("size", 0LL);
    e.channelId = j.value("channel_id", 0LL);
    e.orderId = j.value("order_id", "");
    e.flags = j.value("flags", 0);
    e.tsInDelta = j.value("ts_in_delta", 0LL);
    e.sequence = j.value("sequence", 0ULL);
    e.symbol = j.value("symbol", "");

    return e;
}

std::ostream& operator<<(std::ostream& os, const MarketDataEvent& e)
{
    os << "ts_recv=" << e.tsRecv
        << " ts_event=" << e.tsEvent
        << " instrument_id=" << e.instrumentId
        << " action=" << e.action
        << " side=" << e.side
        << " price=" << e.priceOrNull()
        << " size=" << e.size
        << " order_id=" << e.orderId
        << " sequence=" << e.sequence
        << " symbol=" << e.symbol;

    return os;
}