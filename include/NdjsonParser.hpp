#pragma once

#include "MarketDataEvent.hpp"
#include <string>
#include <optional>
#include <charconv>
#include <cstring>
#include <cstdlib>

/**
 * @brief Parser for the REAL Databento MBO NDJSON format.
 *
 * Actual wire format (from xeur-eobi-*.mbo.json):
 * {
 *   "ts_recv":  "2026-03-09T07:52:41.368148840Z",   <- ISO-8601 string
 *   "hd": {
 *     "ts_event":     "2026-03-09T07:52:41.367824437Z",
 *     "rtype":        160,
 *     "publisher_id": 101,
 *     "instrument_id":34513
 *   },
 *   "action":   "A",
 *   "side":     "B",
 *   "price":    "0.021200000",   <- decimal string (or null)
 *   "size":     20,
 *   "order_id": "10996414798222631105",
 *   "flags":    0,
 *   "symbol":   "EUCO SI 20260710 PS EU P 1.1650 0"
 * }
 */
class NdjsonParser {
public:
    static std::optional<MarketDataEvent> parse_line(std::string_view line) {
        if (line.empty() || line[0] != '{') return std::nullopt;

        MarketDataEvent evt;

        // ── Timestamps (ISO-8601 → nanoseconds) ──────────────────────────────
        if (!extract_iso_ts(line, "\"ts_recv\"",  evt.ts_recv))  return std::nullopt;
        extract_iso_ts(line, "\"ts_event\"", evt.ts_event);  // inside "hd"

        // ── Fields inside "hd" ────────────────────────────────────────────────
        extract_uint32(line, "\"publisher_id\"",  evt.publisher_id);
        extract_uint32(line, "\"instrument_id\"", evt.instrument_id);

        // ── Top-level fields ──────────────────────────────────────────────────
        extract_char  (line, "\"action\"", evt.action);
        extract_char  (line, "\"side\"",   evt.side);
        extract_uint32(line, "\"size\"",   evt.size);
        extract_uint8 (line, "\"flags\"",  evt.flags);

        // order_id is a quoted uint64
        extract_uint64_str(line, "\"order_id\"", evt.order_id);

        // price is a quoted decimal string or null → convert to fixed-point int64
        extract_price(line, "\"price\"", evt.price);

        // symbol optional
        extract_string(line, "\"symbol\"", evt.symbol);

        return evt;
    }

private:
    // ── Find key, return pointer to value (after colon+space) ─────────────────
    static const char* find_key(std::string_view doc, std::string_view key) {
        auto pos = doc.find(key);
        if (pos == std::string_view::npos) return nullptr;
        const char* p = doc.data() + pos + key.size();
        while (*p == ' ' || *p == ':') ++p;
        return p;
    }

    // ── ISO-8601 "2026-03-09T07:52:41.368148840Z" → uint64 nanoseconds ────────
    static bool extract_iso_ts(std::string_view doc, std::string_view key, uint64_t& out) {
        const char* p = find_key(doc, key);
        if (!p || *p != '"') return false;
        ++p; // skip opening quote

        // Parse: YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ
        int year=0, mon=0, day=0, hour=0, min=0, sec=0;
        unsigned long long frac = 0;

        auto parse_int = [](const char* s, int n, int& val) {
            val = 0;
            for (int i = 0; i < n; i++) val = val*10 + (s[i]-'0');
            return s + n;
        };

        p = parse_int(p, 4, year); if (*p++ != '-') return false;
        p = parse_int(p, 2, mon);  if (*p++ != '-') return false;
        p = parse_int(p, 2, day);  if (*p++ != 'T') return false;
        p = parse_int(p, 2, hour); if (*p++ != ':') return false;
        p = parse_int(p, 2, min);  if (*p++ != ':') return false;
        p = parse_int(p, 2, sec);

        // Fractional seconds (up to 9 digits)
        frac = 0;
        int frac_digits = 0;
        if (*p == '.') {
            ++p;
            while (*p >= '0' && *p <= '9' && frac_digits < 9) {
                frac = frac * 10 + (*p++ - '0');
                ++frac_digits;
            }
            // Pad to 9 digits
            while (frac_digits++ < 9) frac *= 10;
        }

        // Convert to Unix timestamp (days since epoch)
        // Simple approach: use mktime-like calculation
        // Days from 1970-01-01
        static const int days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        long long days = 0;
        for (int y = 1970; y < year; y++)
            days += (y%4==0 && (y%100!=0 || y%400==0)) ? 366 : 365;
        bool leap = (year%4==0 && (year%100!=0 || year%400==0));
        for (int m = 1; m < mon; m++) {
            days += days_in_month[m-1];
            if (m == 2 && leap) days++;
        }
        days += day - 1;

        uint64_t total_secs = (uint64_t)days * 86400ULL
                            + (uint64_t)hour * 3600ULL
                            + (uint64_t)min  * 60ULL
                            + (uint64_t)sec;
        out = total_secs * 1'000'000'000ULL + frac;
        return true;
    }

    // ── Quoted decimal price → fixed-point int64 (×1e9) ──────────────────────
    // "0.021200000" → 21200000  (i.e. 0.0212 × 1e9 = 21200000)
    // null          → INT64_MAX (UNDEF_PRICE)
    static bool extract_price(std::string_view doc, std::string_view key, int64_t& out) {
        const char* p = find_key(doc, key);
        if (!p) { out = std::numeric_limits<int64_t>::max(); return false; }

        // null price
        if (p[0]=='n' && p[1]=='u' && p[2]=='l' && p[3]=='l') {
            out = std::numeric_limits<int64_t>::max();
            return true;
        }

        bool negative = false;
        if (*p == '"') ++p;
        if (*p == '-') { negative = true; ++p; }

        // Integer part
        int64_t integer = 0;
        while (*p >= '0' && *p <= '9') integer = integer*10 + (*p++ - '0');

        // Fractional part → exactly 9 decimal places
        int64_t frac = 0;
        int digits = 0;
        if (*p == '.') {
            ++p;
            while (*p >= '0' && *p <= '9' && digits < 9) {
                frac = frac*10 + (*p++ - '0');
                ++digits;
            }
            while (digits++ < 9) frac *= 10;
        }

        out = integer * 1'000'000'000LL + frac;
        if (negative) out = -out;
        return true;
    }

    // ── Quoted uint64 ("10996414798222631105") ────────────────────────────────
    static bool extract_uint64_str(std::string_view doc, std::string_view key, uint64_t& out) {
        const char* p = find_key(doc, key);
        if (!p) return false;
        if (*p == '"') ++p;
        auto [ptr, ec] = std::from_chars(p, doc.data() + doc.size(), out);
        return ec == std::errc{};
    }

    static bool extract_uint32(std::string_view doc, std::string_view key, uint32_t& out) {
        const char* p = find_key(doc, key);
        if (!p) return false;
        if (*p == '"') ++p;
        auto [ptr, ec] = std::from_chars(p, doc.data() + doc.size(), out);
        return ec == std::errc{};
    }

    static bool extract_uint8(std::string_view doc, std::string_view key, uint8_t& out) {
        const char* p = find_key(doc, key);
        if (!p) return false;
        unsigned tmp = 0;
        auto [ptr, ec] = std::from_chars(p, doc.data() + doc.size(), tmp);
        out = static_cast<uint8_t>(tmp);
        return ec == std::errc{};
    }

    static bool extract_char(std::string_view doc, std::string_view key, char& out) {
        const char* p = find_key(doc, key);
        if (!p) return false;
        if (*p == '"') ++p;
        out = *p;
        return true;
    }

    static bool extract_string(std::string_view doc, std::string_view key, std::string& out) {
        const char* p = find_key(doc, key);
        if (!p || *p != '"') return false;
        ++p;
        const char* end = p;
        while (*end && *end != '"') ++end;
        out.assign(p, end);
        return true;
    }
};
