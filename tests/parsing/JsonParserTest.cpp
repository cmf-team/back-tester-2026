#include "TestSupport.hpp"

#include "parsing/JsonParser.hpp"

#include <limits>
#include <string>
#include <vector>

namespace md::test {
namespace {

std::string rootTimestampLine(
    std::uint64_t ts_event,
    const std::string& order_id_json,
    const std::string& instrument_id_json
) {
    return "{\"ts_event\":" + std::to_string(ts_event)
        + ",\"header\":{\"instrument_id\":" + instrument_id_json
        + "},\"action\":\"A\",\"side\":\"B\",\"price\":1.250000000,\"size\":10,\"order_id\":"
        + order_id_json + "}";
}

} // namespace

void testParserValidEvent() {
    const auto parsed = parseMarketDataEventLine(line(100, 42), 1, 7, 11);
    require(parsed.timestamp == xeur_base_timestamp + 100, "timestamp parsed");
    require(parsed.ts_recv == xeur_base_timestamp + 100, "ts_recv parsed");
    require(parsed.ts_event == xeur_base_timestamp + 100, "hd ts_event parsed");
    require(parsed.order_id == 42, "order_id parsed");
    require(parsed.instrument_id == 442, "instrument_id parsed");
    require(parsed.side == Side::Bid, "side parsed");
    require(parsed.action == Action::Add, "action parsed");
    require(parsed.price == 1085000000, "string price parsed");
    require(parsed.size == 10, "size parsed");
    require(parsed.source_file_id == 7, "source file id preserved");
    require(parsed.source_sequence == 11, "source sequence preserved");
    require(parsed.line_number == 1, "line number preserved");
}

void testParserNullAndDecimalPrices() {
    const auto null_price = parseMarketDataEventLine(line(100, 1, 'A', 'C', "null"), 1);
    require(null_price.price == std::numeric_limits<std::int64_t>::max(), "null price maps to UNDEF");

    const auto string_decimal = parseMarketDataEventLine(line(100, 1, 'B', 'M', "\"1.250000000\""), 1);
    require(string_decimal.price == 1250000000, "string decimal price converted to fixed int64");

    const auto numeric_decimal = parseMarketDataEventLine(line(100, 1, 'B', 'M', "1.250000000"), 1);
    require(numeric_decimal.price == 1250000000, "numeric decimal price converted to fixed int64");

    const auto negative_decimal = parseMarketDataEventLine(line(100, 1, 'B', 'M', "\"-1.125000000\""), 1);
    require(negative_decimal.price == -1125000000, "negative decimal price converted to fixed int64");
}

void testParserTimestampFallbackAndNumericFields() {
    const auto fallback = parseMarketDataEventLine(rootTimestampLine(123456789, "987654321", "\"777\""), 3);
    require(fallback.ts_recv == 0, "missing ts_recv stays empty");
    require(fallback.ts_event == 123456789, "root numeric ts_event parsed");
    require(fallback.timestamp == 123456789, "timestamp falls back to ts_event");
    require(fallback.order_id == 987654321, "numeric order_id parsed");
    require(fallback.instrument_id == 777, "header alias instrument_id parsed from string");
    require(fallback.price == 1250000000, "numeric fallback fixture price parsed");

    const std::string nested_hd =
        "{\"ts_recv\":\"2026-03-09T00:03:00.129732099Z\","
        "\"hd\":{\"ts_event\":\"2026-03-09T00:00:00.000000000Z\","
        "\"rtype\":160,\"publisher_id\":101,\"instrument_id\":442},"
        "\"action\":\"R\","
        "\"side\":\"N\","
        "\"price\":null,"
        "\"size\":0,"
        "\"channel_id\":23,"
        "\"order_id\":\"0\","
        "\"flags\":8,"
        "\"ts_in_delta\":0,"
        "\"sequence\":0,"
        "\"symbol\":\"FCEU SI 20260316 PS\"}";

    const auto parsed = parseMarketDataEventLine(nested_hd, 1);
    require(parsed.timestamp == 1773014580129732099ULL, "databento ts_recv ISO parsed");
    require(parsed.ts_recv == 1773014580129732099ULL, "databento ts_recv stored");
    require(parsed.ts_event == 1773014400000000000ULL, "databento hd.ts_event parsed");
    require(parsed.instrument_id == 442, "databento hd.instrument_id parsed");
    require(parsed.order_id == 0, "databento string order_id parsed");
}

void testParserSideAndActionCodes() {
    const std::vector<std::pair<char, Side>> side_cases{
        {'A', Side::Ask},
        {'B', Side::Bid},
        {'N', Side::None},
        {'?', Side::None},
    };

    for (const auto& [code, expected] : side_cases) {
        const auto parsed = parseMarketDataEventLine(line(100, 1, code, 'A'), 1);
        require(parsed.side == expected, std::string{"side code parsed: "} + code);
    }

    const std::vector<std::pair<char, Action>> action_cases{
        {'A', Action::Add},
        {'M', Action::Modify},
        {'C', Action::Cancel},
        {'R', Action::Clear},
        {'T', Action::Trade},
        {'F', Action::Fill},
        {'?', Action::None},
    };

    for (const auto& [code, expected] : action_cases) {
        const auto parsed = parseMarketDataEventLine(line(100, 1, 'B', code), 1);
        require(parsed.action == expected, std::string{"action code parsed: "} + code);
    }
}

} // namespace md::test
