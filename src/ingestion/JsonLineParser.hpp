#pragma once

#include "common/MarketDataEvent.hpp"
#include "ingestion/NanosParser.hpp"
#include <charconv>
#include <cstring>
#include <optional>
#include <string_view>
#include <string.h>

namespace cmf {

// Single-pass NDJSON parser for Databento MBO schema.
// Actual document field order (confirmed from data):
//   ts_recv -> hd:{ts_event, rtype, publisher_id, instrument_id} ->
//   action -> side -> price -> size -> channel_id -> order_id ->
//   flags -> ts_in_delta -> sequence -> symbol
// memmem advances cursor forward on each call — never rescans from line start.
inline std::optional<MarketDataEvent> parse_mbo_line(std::string_view line) noexcept {
    if (line.empty() || line.front() != '{') return std::nullopt;

    const char* p   = line.data();
    const char* end = p + line.size();

    auto skip = [&](std::string_view key) -> bool {
        const void* found = memmem(p, static_cast<std::size_t>(end - p), key.data(), key.size());
        if (!found) return false;
        p = static_cast<const char*>(found) + key.size();
        return true;
    };

    MarketDataEvent e{};

    if (!skip(R"("ts_recv":")")) return std::nullopt;
    if (p + 31 > end) return std::nullopt;
    e.ts_recv = parse_iso8601_ns(std::string_view(p, 30));
    p += 31;

    if (!skip(R"("ts_event":")")) return std::nullopt;
    if (p + 31 > end) return std::nullopt;
    e.ts_event = parse_iso8601_ns(std::string_view(p, 30));
    p += 31;

    if (!skip(R"("rtype":)")) return std::nullopt;
    p = std::from_chars(p, end, e.rtype).ptr;

    if (!skip(R"("publisher_id":)")) return std::nullopt;
    p = std::from_chars(p, end, e.publisher_id).ptr;

    if (!skip(R"("instrument_id":)")) return std::nullopt;
    p = std::from_chars(p, end, e.instrument_id).ptr;

    if (!skip(R"("action":")")) return std::nullopt;
    if (p + 2 > end) return std::nullopt;
    e.action = *p; p += 2;

    if (!skip(R"("side":")")) return std::nullopt;
    if (p + 2 > end) return std::nullopt;
    e.side = *p; p += 2;

    if (!skip(R"("price":)")) return std::nullopt;
    if (p + 4 <= end && p[0] == 'n') {
        p += 4;
    } else if (p < end && *p == '"') {
        ++p;
        const char* q = static_cast<const char*>(memchr(p, '"', static_cast<std::size_t>(end - p)));
        if (q) { std::from_chars(p, q, e.price); p = q + 1; }
    } else if (p < end) {
        auto [ptr, ec] = std::from_chars(p, end, e.price);
        if (ec == std::errc{}) p = ptr;
    }

    if (!skip(R"("size":)")) return std::nullopt;
    p = std::from_chars(p, end, e.size).ptr;

    if (!skip(R"("channel_id":)")) return std::nullopt;
    p = std::from_chars(p, end, e.channel_id).ptr;

    if (!skip(R"("order_id":")")) return std::nullopt;
    {
        const char* q = static_cast<const char*>(memchr(p, '"', static_cast<std::size_t>(end - p)));
        if (q) { std::from_chars(p, q, e.order_id); p = q + 1; }
    }

    if (!skip(R"("flags":)")) return std::nullopt;
    p = std::from_chars(p, end, e.flags).ptr;

    if (!skip(R"("ts_in_delta":)")) return std::nullopt;
    p = std::from_chars(p, end, e.ts_in_delta).ptr;

    if (!skip(R"("sequence":)")) return std::nullopt;
    p = std::from_chars(p, end, e.sequence).ptr;

    if (!skip(R"("symbol":")")) return std::nullopt;
    {
        const char* q = static_cast<const char*>(memchr(p, '"', static_cast<std::size_t>(end - p)));
        if (q) {
            std::size_t n = std::min<std::size_t>(static_cast<std::size_t>(q - p), sizeof(e.symbol) - 1);
            std::memcpy(e.symbol, p, n);
            e.symbol[n] = '\0';
        }
    }

    return e;
}

} // namespace cmf
