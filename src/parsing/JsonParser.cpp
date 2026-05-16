#include "parsing/JsonParser.hpp"

#include "simdjson.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

namespace md {
    namespace {
        constexpr std::int64_t undef_price = std::numeric_limits<std::int64_t>::max();
        constexpr std::int64_t fixed_point_scale = 1'000'000'000LL;
        constexpr int fixed_point_digits = 9;

        thread_local simdjson::ondemand::parser parser_instance;
        thread_local std::vector<char> padded_buffer;

        void copyToPaddedBuffer(std::string_view line) {
            const std::size_t needed = line.size() + simdjson::SIMDJSON_PADDING;
            if (padded_buffer.size() < needed) padded_buffer.assign(needed, '\0');
            std::memcpy(padded_buffer.data(), line.data(), line.size());
            std::memset(padded_buffer.data() + line.size(), 0, simdjson::SIMDJSON_PADDING);
        }

        std::string_view trimJsonToken(std::string_view token) {
            while (!token.empty() && (token.back() == ' ' || token.back() == '\t' || token.back() == '\r' || token.back() == '\n')) {
                token.remove_suffix(1);
            }
            return token;
        }

        std::string_view unquoteJsonToken(std::string_view token) {
            if (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
                return token.substr(1, token.size() - 2);
            }
            return token;
        }

        std::uint64_t toUInt64(std::string_view t) {
            std::uint64_t v = 0;
            std::from_chars(t.data(), t.data() + t.size(), v);
            return v;
        }

        std::uint64_t isoUtcToNs(std::string_view t) {
            using namespace std::chrono;
            auto d2 = [&](std::size_t p) { return (t[p] - '0') * 10 + (t[p + 1] - '0'); };
            auto d4 = [&](std::size_t p) { return d2(p) * 100 + d2(p + 2); };
            const auto whole = sys_days{year{d4(0)} / d2(5) / d2(8)} + hours{d2(11)} + minutes{d2(14)} + seconds{d2(17)};
            std::uint64_t frac = 0;
            int n = 0;
            for (std::size_t p = 20; p < t.size() && n < fixed_point_digits; ++p, ++n) {
                frac = frac * 10 + (t[p] - '0');
            }
            return static_cast<std::uint64_t>(duration_cast<nanoseconds>(whole.time_since_epoch()).count()) + frac;
        }

        std::int64_t readPrice(simdjson::ondemand::value v) {
            const std::string_view token = trimJsonToken(v.raw_json_token());
            if (token == "null") return undef_price;
            return parsePriceText(unquoteJsonToken(token));
        }

        std::uint64_t readUInt64Token(simdjson::ondemand::value value) {
            const std::string_view token = trimJsonToken(value.raw_json_token());
            return toUInt64(unquoteJsonToken(token));
        }

        void readTimestamp(simdjson::ondemand::value value, std::uint64_t& target) {
            const std::string_view token = trimJsonToken(value.raw_json_token());
            if (!token.empty() && token.front() == '"') {
                target = parseTimestampText(unquoteJsonToken(token));
            } else {
                target = toUInt64(token);
            }
        }

        void readStringUInt64(simdjson::ondemand::value value, std::uint64_t& target) {
            target = readUInt64Token(value);
        }

        void readNumberUInt64(simdjson::ondemand::value value, std::uint64_t& target) {
            target = readUInt64Token(value);
        }

        void readSide(simdjson::ondemand::value value, Side& target) {
            target = parseSideText(value.get_string());
        }

        void readAction(simdjson::ondemand::value value, Action& target) {
            target = parseActionText(value.get_string());
        }

        void readHeader(simdjson::ondemand::value value, MarketDataEvent& event) {
            simdjson::ondemand::object header = value.get_object();
            for (auto sub: header) {
                const std::string_view key = sub.unescaped_key();
                if (key == "ts_event") {
                    readTimestamp(sub.value(), event.ts_event);
                } else if (key == "instrument_id") {
                    readNumberUInt64(sub.value(), event.instrument_id);
                }
            }
        }

        void readField(std::string_view key, simdjson::ondemand::value value, MarketDataEvent& event) {
            if (key == "hd" || key == "header") {
                readHeader(value, event);
            } else if (key == "price") {
                event.price = readPrice(value);
            } else if (key == "ts_recv") {
                readTimestamp(value, event.ts_recv);
            } else if (key == "ts_event") {
                readTimestamp(value, event.ts_event);
            } else if (key == "instrument_id") {
                readNumberUInt64(value, event.instrument_id);
            } else if (key == "order_id") {
                readStringUInt64(value, event.order_id);
            } else if (key == "size") {
                readNumberUInt64(value, event.size);
            } else if (key == "side") {
                readSide(value, event.side);
            } else if (key == "action") {
                readAction(value, event.action);
            }
        }
    }

    std::uint64_t parseTimestampText(std::string_view text) {
        return text.find('T') == std::string_view::npos
            ? toUInt64(text)
            : isoUtcToNs(text);
    }

    std::uint64_t parseUInt64Text(std::string_view text) {
        return toUInt64(text);
    }

    std::int64_t parsePriceText(std::string_view text) {
        const bool negative = !text.empty() && text.front() == '-';
        std::size_t pos = negative ? 1 : 0;
        const bool has_decimal_point = text.find('.', pos) != std::string_view::npos;

        std::int64_t whole = 0;
        while (pos < text.size() && text[pos] != '.') {
            whole = whole * 10 + (text[pos] - '0');
            ++pos;
        }

        if (!has_decimal_point) {
            return negative ? -whole : whole;
        }

        std::int64_t frac = 0;
        int digits = 0;
        if (pos < text.size() && text[pos] == '.') {
            ++pos;
            while (pos < text.size() && digits < fixed_point_digits) {
                frac = frac * 10 + (text[pos] - '0');
                ++pos;
                ++digits;
            }
        }
        while (digits++ < fixed_point_digits) {
            frac *= 10;
        }

        const std::int64_t price = whole * fixed_point_scale + frac;
        return negative ? -price : price;
    }

    Side parseSideText(std::string_view text) {
        if (text.empty()) {
            return Side::None;
        }
        switch (text.front()) {
            case 'A': return Side::Ask;
            case 'B': return Side::Bid;
            default: return Side::None;
        }
    }

    Action parseActionText(std::string_view text) {
        if (text.empty()) {
            return Action::None;
        }
        switch (text.front()) {
            case 'A': return Action::Add;
            case 'M': return Action::Modify;
            case 'C': return Action::Cancel;
            case 'R': return Action::Clear;
            case 'T': return Action::Trade;
            case 'F': return Action::Fill;
            default: return Action::None;
        }
    }

    MarketDataEvent parseMarketDataEventLine(std::string_view line, std::size_t line_number) {
        return parseMarketDataEventLine(line, line_number, 0, static_cast<std::uint64_t>(line_number));
    }

    MarketDataEvent parseMarketDataEventLine(
        std::string_view line, std::size_t line_number,
        std::uint32_t source_file_id, std::uint64_t source_sequence
    ) {
        copyToPaddedBuffer(line);
        MarketDataEvent event{
            .price = undef_price,
            .source_file_id = source_file_id, .source_sequence = source_sequence, .line_number = line_number,
        };
        simdjson::ondemand::document doc = parser_instance.iterate(padded_buffer.data(), line.size(), padded_buffer.size());
        for (auto field : doc.get_object()) {
            std::string_view key = field.unescaped_key();
            readField(key, field.value(), event);
        }
        event.timestamp = event.ts_recv != 0 ? event.ts_recv : event.ts_event;
        return event;
    }
}
