#pragma once

#include "common/MarketDataEvent.hpp"
#include "ingestion/NanosParser.hpp"
#include <charconv>
#include <cstring>
#include <optional>
#include <string_view>

namespace cmf {

namespace detail {

inline std::string_view str_val(std::string_view line, std::string_view key) noexcept {
    auto p = line.find(key);
    if (p == std::string_view::npos) return {};
    p += key.size();
    auto e = line.find('"', p);
    if (e == std::string_view::npos) return {};
    return line.substr(p, e - p);
}

template <typename T>
inline T num_val(std::string_view line, std::string_view key) noexcept {
    auto p = line.find(key);
    if (p == std::string_view::npos) return T{};
    p += key.size();
    T v{};
    std::from_chars(line.data() + p, line.data() + line.size(), v);
    return v;
}

} // namespace detail

// Parse one NDJSON line from the Databento MBO schema.
inline std::optional<MarketDataEvent> parse_mbo_line(std::string_view line) noexcept {
    if (line.empty() || line.front() != '{') return std::nullopt;

    MarketDataEvent e{};

    auto ts_recv_sv  = detail::str_val(line, R"("ts_recv":")");
    auto ts_event_sv = detail::str_val(line, R"("ts_event":")");
    if (ts_recv_sv.empty() || ts_event_sv.empty()) return std::nullopt;

    e.ts_recv       = parse_iso8601_ns(ts_recv_sv);
    e.ts_event      = parse_iso8601_ns(ts_event_sv);
    e.rtype         = detail::num_val<uint8_t> (line, R"("rtype":)");
    e.publisher_id  = detail::num_val<uint32_t>(line, R"("publisher_id":)");
    e.instrument_id = detail::num_val<uint32_t>(line, R"("instrument_id":)");
    e.sequence      = detail::num_val<uint32_t>(line, R"("sequence":)");
    e.size          = detail::num_val<uint32_t>(line, R"("size":)");
    e.flags         = detail::num_val<uint8_t> (line, R"("flags":)");
    e.ts_in_delta   = detail::num_val<int32_t> (line, R"("ts_in_delta":)");
    e.channel_id    = detail::num_val<uint16_t>(line, R"("channel_id":)");

    auto oid = detail::str_val(line, R"("order_id":")");
    if (!oid.empty())
        std::from_chars(oid.data(), oid.data() + oid.size(), e.order_id);

    auto act = detail::str_val(line, R"("action":")");
    if (!act.empty()) e.action = act[0];

    auto sid = detail::str_val(line, R"("side":")");
    if (!sid.empty()) e.side = sid[0];

    {
        auto p = line.find(R"("price":)");
        if (p != std::string_view::npos) {
            p += 8;
            if (line.substr(p, 4) != "null") {
                auto q = line.find('"', p);
                if (q != std::string_view::npos) {
                    auto end = line.find('"', q + 1);
                    if (end != std::string_view::npos)
                        std::from_chars(line.data() + q + 1, line.data() + end, e.price);
                }
            }
        }
    }

    auto sym = detail::str_val(line, R"("symbol":")");
    if (!sym.empty()) {
        std::size_t n = std::min(sym.size(), sizeof(e.symbol) - 1);
        std::memcpy(e.symbol, sym.data(), n);
        e.symbol[n] = '\0';
    }

    return e;
}

} // namespace cmf
