#pragma once

#include "common/BasicTypes.hpp"
#include <string_view>

namespace cmf {

static int64_t days_from_civil(int64_t y, int64_t m, int64_t d) noexcept {
    y -= (m <= 2);
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m <= 2 ? 9 : -3)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// Parse "YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ" -> nanoseconds since Unix epoch.
// Databento always provides 9-digit nanosecond fractions; input is exactly 30 chars.
inline NanoTime parse_iso8601_ns(std::string_view s) noexcept {
    const char* p = s.data();
    auto ri = [p](int off, int n) noexcept -> int64_t {
        int64_t v = 0;
        for (int i = 0; i < n; i++) v = v * 10 + (p[off + i] - '0');
        return v;
    };

    int64_t epoch_sec = days_from_civil(ri(0, 4), ri(5, 2), ri(8, 2)) * 86400LL
                        + ri(11, 2) * 3600LL + ri(14, 2) * 60LL + ri(17, 2);

    // 9 nanosecond digits split as 4+4+1 to avoid a 9-iteration loop the compiler won't fully unroll
    int64_t nanos = ri(20, 4) * 100000LL + ri(24, 4) * 10LL + (p[28] - '0');

    return static_cast<NanoTime>(epoch_sec) * 1'000'000'000LL + nanos;
}

} // namespace cmf
