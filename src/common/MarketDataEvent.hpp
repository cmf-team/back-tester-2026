#pragma once

#include "common/BasicTypes.hpp"
#include <cstdint>
#include <cstring>
#include <limits>

namespace cmf {

struct MarketDataEvent {
    NanoTime  ts_recv       = 0;
    NanoTime  ts_event      = 0;
    uint64_t  order_id      = 0;
    double    price         = std::numeric_limits<double>::quiet_NaN();
    uint32_t  instrument_id = 0;
    uint32_t  publisher_id  = 0;
    uint32_t  sequence      = 0;
    uint32_t  size          = 0;
    int32_t   ts_in_delta   = 0;
    uint16_t  channel_id    = 0;
    uint8_t   rtype         = 0;
    uint8_t   flags         = 0;
    char      action        = 'N';
    char      side          = 'N';
    char      symbol[66]    = {};

    bool operator<(const MarketDataEvent& o) const noexcept { return ts_recv < o.ts_recv; }
    bool operator>(const MarketDataEvent& o) const noexcept { return ts_recv > o.ts_recv; }
};

} // namespace cmf
