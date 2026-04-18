#include "Parser.hpp"

namespace cmf {

std::string getStr(const json& j, const std::string& key,
                   const std::string& def) {
    if (j.contains(key) && j[key].is_string())
        return j[key].get<std::string>();
    return def;
}

MarketDataEvent parseLine(const std::string& line) {
    json j = json::parse(line);

    MarketDataEvent event;

    event.tsRecvStr = getStr(j, "ts_recv");

    if (j.contains("hd")) {
        auto& hd = j["hd"];
        event.tsEventStr   = getStr(hd, "ts_event");
        event.rtype        = hd.value("rtype", 0);
        event.publisherId  = hd.value("publisher_id", 0);
        event.instrumentId = hd.value("instrument_id", 0);
    }

    std::string a = getStr(j, "action", "N");
    std::string s = getStr(j, "side", "N");
    event.action = a[0];
    event.side   = s[0];

    event.priceStr   = getStr(j, "price");
    event.size       = j.value("size", 0);
    event.orderIdStr = getStr(j, "order_id");
    event.flags      = j.value("flags", 0);
    event.tsInDelta  = j.value("ts_in_delta", 0);
    event.sequence   = j.value("sequence", 0);
    event.symbol     = getStr(j, "symbol");

    return event;
}

}