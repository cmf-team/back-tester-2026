#define _GNU_SOURCE

#include "JSONParser.hpp"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace cmf::parser::json {

// Returns pointer to the start of a value for the given key, or nullptr.
static const char* find_key(const char* p, const char* end,
                             const char* key, size_t klen) {
    for (; p + klen + 1 < end; ++p) {
        if (p[0] == '"'
            && std::memcmp(p + 1, key, klen) == 0
            && p[klen + 1] == '"') {
            const char* after = p + klen + 2;
            if (after < end && *after == ':') {
                ++after;
                while (after < end && *after == ' ') ++after;
                return after;
            }
        }
    }
    return nullptr;
}

// Returns a view into the original buffer — no copies.
// Strips outer quotes for strings; returns empty view for null.
static std::string_view read_value(const char* p, const char* end) {
    if (!p || p >= end) return {};
    if (*p == '"') {
        const char* s = p + 1;
        const char* e = s;
        while (e < end && *e != '"') ++e;
        return {s, static_cast<size_t>(e - s)};
    }
    if (*p == 'n') return {};  // null
    const char* s = p;
    while (p < end && *p != ',' && *p != '}' && *p != ' ') ++p;
    return {s, static_cast<size_t>(p - s)};
}

template<typename T>
static T parse_uint(const char* p, const char* end) {
    T v = 0;
    while (p < end && *p >= '0' && *p <= '9')
        v = static_cast<T>(v * 10 + (*p++ - '0'));
    return v;
}

static double parse_double(const char* p, const char* end) {
    if (p >= end) return 0.0;
    bool neg = (*p == '-');
    if (neg) ++p;
    std::int64_t integer = 0;
    while (p < end && *p >= '0' && *p <= '9')
        integer = integer * 10 + (*p++ - '0');
    std::int64_t frac = 0;
    int frac_digits = 0;
    if (p < end && *p == '.') {
        ++p;
        while (p < end && *p >= '0' && *p <= '9') {
            frac = frac * 10 + (*p++ - '0');
            ++frac_digits;
        }
    }
    static constexpr double pow10[] = {
        1, 10, 100, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12
    };
    double result = static_cast<double>(integer)
                  + static_cast<double>(frac) / pow10[frac_digits];
    return neg ? -result : result;
}


// Parses "2026-03-09T07:52:41.368148840Z" directly from raw bytes.
static NanoTime parse_timestamp(const char* p, size_t len) {
    if (len < 19) return 0;

    auto r2 = [](const char* s) { return (s[0] - '0') * 10 + (s[1] - '0'); };
    auto r4 = [](const char* s) {
        return (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
    };

    std::tm t{};
    t.tm_year = r4(p)      - 1900;
    t.tm_mon  = r2(p +  5) - 1;
    t.tm_mday = r2(p +  8);
    t.tm_hour = r2(p + 11);
    t.tm_min  = r2(p + 14);
    t.tm_sec  = r2(p + 17);

    std::int64_t nanos = 0;
    if (len > 20 && p[19] == '.') {
        const char* f    = p + 20;
        const char* fend = f;
        while (fend < p + len && *fend != 'Z') ++fend;
        for (size_t i = 0; i < 9; ++i)
            nanos = nanos * 10 + (f + i < fend ? f[i] - '0' : 0);
    }

    return static_cast<NanoTime>(timegm(&t)) * 1'000'000'000LL + nanos;
}

} // namespace cmf::parser::json

namespace cmf::parser {

MarketDataEvent JSONParser::parse_line(const std::string& line) {

    const char* buf = line.data();
    const char* end = buf + line.size();

    auto get = [&](const char* key, size_t klen) {
        return json::read_value(json::find_key(buf, end, key, klen), end);
    };

    MarketDataEvent ev{};

    auto sv = get("ts_recv", 7);
    ev.ts_received = json::parse_timestamp(sv.data(), sv.size());

    sv = get("ts_event", 8);
    ev.ts_event = json::parse_timestamp(sv.data(), sv.size());

    sv = get("instrument_id", 13);
    ev.instrument_id = json::parse_uint<SecurityId>(sv.data(), sv.data() + sv.size());

    sv = get("publisher_id", 12);
    ev.market_id = json::parse_uint<MarketId>(sv.data(), sv.data() + sv.size());

    sv = get("channel_id", 10);
    ev.channel_id = json::parse_uint<std::uint32_t>(sv.data(), sv.data() + sv.size());

    sv = get("order_id", 8);
    ev.order_id = json::parse_uint<OrderId>(sv.data(), sv.data() + sv.size());

    sv = get("sequence", 8);
    ev.sequence = json::parse_uint<std::uint32_t>(sv.data(), sv.data() + sv.size());

    sv = get("flags", 5);
    ev.flags = json::parse_uint<std::uint8_t>(sv.data(), sv.data() + sv.size());

    sv = get("size", 4);
    ev.qty = json::parse_double(sv.data(), sv.data() + sv.size());

    sv = get("price", 5);
    if (!sv.empty())
        ev.price = json::parse_double(sv.data(), sv.data() + sv.size());

    sv = get("symbol", 6);
    ev.symbol.assign(sv.data(), sv.size());

    sv = get("action", 6);
    if (!sv.empty()) ev.action = action::from_char(sv[0]);

    sv = get("side", 4);
    if (!sv.empty()) ev.side = side::from_char(sv[0]);

    return ev;
}

} // namespace cmf::parser
