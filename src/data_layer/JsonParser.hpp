#pragma once
#include "common/MarketDataEvent.hpp"
#include "common/BasicTypes.hpp"
#include <optional>
#include <charconv>
#include <string_view>
#include <cstring>

namespace cmf {

static inline int64_t days_from_civil(int64_t y, int64_t m, int64_t d) noexcept {
    y -= (m <= 2);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m <= 2 ? 9 : -3)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

inline NanoTime parse_iso8601_ns(std::string_view s) noexcept {
    const char* p = s.data();

    auto ri = [&](int off, int n) noexcept -> int64_t {
        const char* q = p + off;
        int64_t v = 0;
        for (int i = 0; i < n; ++i)
            v = v * 10 + static_cast<uint8_t>(q[i] - '0');
        return v;
    };

    int64_t epoch_sec =
        days_from_civil(ri(0,4), ri(5,2), ri(8,2)) * 86400LL +
        ri(11,2) * 3600LL +
        ri(14,2) * 60LL +
        ri(17,2);

    const char* frac = p + 20;
    int64_t nanos =
        (static_cast<int64_t>(frac[0]-'0') * 1000 +
         static_cast<int64_t>(frac[1]-'0') * 100 +
         static_cast<int64_t>(frac[2]-'0') * 10 +
         static_cast<int64_t>(frac[3]-'0')) * 100000LL +
        (static_cast<int64_t>(frac[4]-'0') * 1000 +
         static_cast<int64_t>(frac[5]-'0') * 100 +
         static_cast<int64_t>(frac[6]-'0') * 10 +
         static_cast<int64_t>(frac[7]-'0')) * 10LL +
        (frac[8] - '0');

    return static_cast<NanoTime>(epoch_sec) * 1'000'000'000LL + nanos;
}

inline std::optional<MarketDataEvent> parse_mbo_line(std::string_view line) noexcept {
    if (line.empty() || line.front() != '{') return std::nullopt;

    const char* p   = line.data();
    const char* end = p + line.size();

    auto expect = [&](std::string_view key) -> bool {
        const size_t len = key.size();
        if (static_cast<size_t>(end - p) < len) return false;
        if (memcmp(p, key.data(), len) != 0) return false;
        p += len;
        return true;
    };

    auto find_next = [&](std::string_view key) -> bool {
        const char* it = static_cast<const char*>(
            memmem(p, static_cast<size_t>(end - p), key.data(), key.size())
        );
        if (!it) return false;
        p = it;
        return true;
    };

    auto parse_int = [&](auto& out) -> bool {
        auto [ptr, ec] = std::from_chars(p, end, out);
        if (ec != std::errc{}) return false;
        p = ptr;
        return true;
    };

    auto parse_str_num = [&](auto& out) -> bool {
        if (p < end && *p == '"') {
            ++p;
            const char* q = static_cast<const char*>(memchr(p, '"', end - p));
            if (!q) return false;
            auto [ptr, ec] = std::from_chars(p, q, out);
            if (ec != std::errc{}) return false;
            p = q + 1;
            return true;
        }
        return parse_int(out);
    };

    MarketDataEvent e{};

    // ts_recv
    if (!find_next(R"("ts_recv":")") || !expect(R"("ts_recv":")")) return std::nullopt;
    if (p + 30 > end) return std::nullopt;
    e.ts_recv = parse_iso8601_ns(std::string_view(p, 30));
    p += 31;

    // ts_event
    if (!find_next(R"("ts_event":")") || !expect(R"("ts_event":")")) return std::nullopt;
    if (p + 30 > end) return std::nullopt;
    e.ts_event = parse_iso8601_ns(std::string_view(p, 30));
    p += 31;

    if (!find_next(R"("rtype":)") || !expect(R"("rtype":)") || !parse_int(e.rtype)) return std::nullopt;
    if (!find_next(R"("publisher_id":)") || !expect(R"("publisher_id":)") || !parse_int(e.publisher_id)) return std::nullopt;
    if (!find_next(R"("instrument_id":)") || !expect(R"("instrument_id":)") || !parse_int(e.instrument_id)) return std::nullopt;

    if (!find_next(R"("action":")") || !expect(R"("action":")")) return std::nullopt;
    if (p + 1 > end) return std::nullopt;
    e.action = *p; p += 2;

    if (!find_next(R"("side":")") || !expect(R"("side":")")) return std::nullopt;
    if (p + 1 > end) return std::nullopt;
    e.side = *p; p += 2;

    // price
    if (!find_next(R"("price":)") || !expect(R"("price":)")) return std::nullopt;
    if (p + 4 <= end && memcmp(p, "null", 4) == 0) {
        p += 4;
    } else if (!parse_str_num(e.price)) {
        return std::nullopt;
    }

    if (!find_next(R"("size":)") || !expect(R"("size":)") || !parse_int(e.size)) return std::nullopt;
    if (!find_next(R"("channel_id":)") || !expect(R"("channel_id":)") || !parse_int(e.channel_id)) return std::nullopt;

    if (!find_next(R"("order_id":")") || !expect(R"("order_id":")")) return std::nullopt;
    {
        const char* q = static_cast<const char*>(memchr(p, '"', end - p));
        if (!q) return std::nullopt;
        auto [ptr, ec] = std::from_chars(p, q, e.order_id);
        if (ec != std::errc{}) return std::nullopt;
        p = q + 1;
    }

    if (!find_next(R"("flags":)") || !expect(R"("flags":)") || !parse_int(e.flags)) return std::nullopt;
    if (!find_next(R"("ts_in_delta":)") || !expect(R"("ts_in_delta":)") || !parse_int(e.ts_in_delta)) return std::nullopt;
    if (!find_next(R"("sequence":)") || !expect(R"("sequence":)") || !parse_int(e.sequence)) return std::nullopt;

    if (!find_next(R"("symbol":")") || !expect(R"("symbol":")")) return std::nullopt;
    {
        const char* q = static_cast<const char*>(memchr(p, '"', end - p));
        if (!q) return std::nullopt;

        std::size_t n = std::min<std::size_t>(q - p, sizeof(e.symbol) - 1);
        std::memcpy(e.symbol, p, n);
        e.symbol[n] = '\0';
    }

    return e;
}

} // namespace cmf