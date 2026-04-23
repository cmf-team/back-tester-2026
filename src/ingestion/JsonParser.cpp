#include "JsonParser.hpp"

#include <string>

namespace cmf {

static Side parseSide(const std::string& s) {
    if (s == "buy") return Side::Buy;
    if (s == "sell") return Side::Sell;
    return Side::None;
}

static Action parseAction(const std::string& s) {
    if (s == "add") return Action::Add;
    if (s == "modify") return Action::Modify;
    if (s == "delete") return Action::Delete;
    if (s == "trade") return Action::Trade;
    return Action::Unknown;
}

// very simple extractor (flat JSON only)
static std::string extract(const std::string& line, const std::string& key) {
    auto pos = line.find(key);
    if (pos == std::string::npos) return "";

    pos = line.find(':', pos);
    if (pos == std::string::npos) return "";

    pos++;

    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '"'))
        pos++;

    size_t end = pos;

    if (line[pos - 1] == '"') {
        while (end < line.size() && line[end] != '"') end++;
    } else {
        while (end < line.size() && line[end] != ',' && line[end] != '}') end++;
    }

    return line.substr(pos, end - pos);
}

std::optional<MarketDataEvent> parseLine(const std::string& line) {
    try {
        MarketDataEvent e;

        auto ts = extract(line, "timestamp");
        if (ts.empty()) return std::nullopt;

        e.timestamp = std::stoll(ts);
        e.orderId = std::stoull(extract(line, "order_id"));
        e.side = parseSide(extract(line, "side"));
        e.price = std::stod(extract(line, "price"));
        e.size = std::stod(extract(line, "size"));
        e.action = parseAction(extract(line, "action"));

        return e;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace cmf