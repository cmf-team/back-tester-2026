#include "JsonLineParser.h"

#include <string>
#include <optional>

// helper функция: достать строковое значение
static std::string extractString(const std::string& line, const std::string& key) {
    auto pos = line.find(key);
    if (pos == std::string::npos) return "";

    pos = line.find(":", pos);
    pos = line.find("\"", pos);
    auto end = line.find("\"", pos + 1);

    return line.substr(pos + 1, end - pos - 1);
}

// helper: число (int)
static std::uint64_t extractUInt(const std::string& line, const std::string& key) {
    auto pos = line.find(key);
    if (pos == std::string::npos) return 0;

    pos = line.find(":", pos);
    auto end = line.find_first_of(",}", pos);

    return std::stoull(line.substr(pos + 1, end - pos - 1));
}

// helper: double (price)
static std::optional<double> extractPrice(const std::string& line) {
    auto pos = line.find("\"price\"");
    if (pos == std::string::npos) return std::nullopt;

    pos = line.find(":", pos);

    if (line.substr(pos, 10).find("null") != std::string::npos) {
        return std::nullopt;
    }

    pos = line.find("\"", pos);
    auto end = line.find("\"", pos + 1);

    return std::stod(line.substr(pos + 1, end - pos - 1));
}

// helper: char (action / side)
static char extractChar(const std::string& line, const std::string& key) {
    auto pos = line.find(key);
    if (pos == std::string::npos) return '\0';

    pos = line.find(":", pos);
    pos = line.find("\"", pos);

    return line[pos + 1];
}

// --- главный parser ---

MarketDataEvent JsonLineParser::parse(const std::string& line) {
    MarketDataEvent e;

    e.ts_recv = extractString(line, "\"ts_recv\"");
    e.ts_event = extractString(line, "\"ts_event\"");

    e.rtype = extractUInt(line, "\"rtype\"");
    e.publisher_id = extractUInt(line, "\"publisher_id\"");
    e.instrument_id = extractUInt(line, "\"instrument_id\"");

    e.action = extractChar(line, "\"action\"");
    e.side = extractChar(line, "\"side\"");

    e.price = extractPrice(line);
    e.size = extractUInt(line, "\"size\"");

    e.channel_id = extractUInt(line, "\"channel_id\"");
    e.order_id = extractString(line, "\"order_id\"");

    e.flags = extractUInt(line, "\"flags\"");
    e.ts_in_delta = extractUInt(line, "\"ts_in_delta\"");
    e.sequence = extractUInt(line, "\"sequence\"");

    e.symbol = extractString(line, "\"symbol\"");

    return e;
}