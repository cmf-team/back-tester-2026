// OrderState — minimal cache entry for an outstanding L3 order.
//
// Stored by the Dispatcher (not the LOB itself) so that Cancel/Modify/Trade/
// Fill messages, which may arrive without an instrument_id, can still be
// resolved to the correct LimitOrderBook and the correct level decrement.

#pragma once

#include "common/BasicTypes.hpp"
#include "market_data/MarketDataEvent.hpp"

#include <cstdint>

namespace cmf
{

struct OrderState
{
    std::uint64_t instrument_id{0};
    Side side{Side::None};
    std::int64_t price{MarketDataEvent::kUndefPrice}; // scaled 1e-9
    std::int64_t qty{0};
};

} // namespace cmf
