#pragma once

#include "common/BasicTypes.hpp"
#include <string_view>

namespace cmf {

// Howard Hinnant's civil-time algorithm (public domain)
static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) noexcept {
    y -= (m <= 2);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m <= 2 ? 9 : -3)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// Parse "YYYY-MM-DDTHH:MM:SS[.nnnnnnnnn]Z" -> nanoseconds since Unix epoch
inline NanoTime parse_iso8601_ns(std::string_view s) noexcept {
    if (s.size() < 19) return 0;
    auto ri = [](const char* p, int n) noexcept -> int64_t {
        int64_t v = 0;
        for (int i = 0; i < n; i++) v = v * 10 + (p[i] - '0');
        return v;
    };
    int64_t year  = ri(s.data() + 0, 4);
    int64_t month = ri(s.data() + 5, 2);
    int64_t day   = ri(s.data() + 8, 2);
    int64_t hour  = ri(s.data() + 11, 2);
    int64_t min   = ri(s.data() + 14, 2);
    int64_t sec   = ri(s.data() + 17, 2);

    int64_t nanos = 0;
    if (s.size() > 20 && s[19] == '.') {
        const char* frac = s.data() + 20;
        int len = 0;
        while (len < 9 && frac[len] >= '0' && frac[len] <= '9') len++;
        for (int i = 0; i < len; i++) nanos = nanos * 10 + (frac[i] - '0');
        for (int i = len; i < 9; i++) nanos *= 10;
    }

    int64_t epoch_sec = days_from_civil(year, month, day) * 86400LL
                        + hour * 3600LL + min * 60LL + sec;
    return static_cast<NanoTime>(epoch_sec) * 1'000'000'000LL + nanos;
}

} // namespace cmf
