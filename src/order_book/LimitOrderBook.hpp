// LimitOrderBook — aggregated L2 book maintained per instrument.
//
// Stateless w.r.t. individual orders: the dispatcher tracks outstanding
// OrderState entries and applies signed deltas via applyDelta(). This keeps
// the LOB free of order_id bookkeeping and lets the dispatcher resolve
// Cancel/Trade/Fill messages that arrive without instrument_id.
//
// Price units are scaled int64 (1 unit == 1e-9), matching MarketDataEvent::price.

#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <optional>

namespace cmf
{

class LimitOrderBook
{
  public:
    struct Bbo
    {
        std::optional<std::int64_t> bid_price; // scaled 1e-9
        std::int64_t bid_size{0};
        std::optional<std::int64_t> ask_price;
        std::int64_t ask_size{0};
    };

    // Adds (qty > 0) or removes (qty < 0) the signed quantity at the given level
    // on the given side. A level with non-positive aggregate size is erased.
    // `qty == 0` is a no-op. Throws std::invalid_argument on Side::None.
    void applyDelta(Side side, std::int64_t price_scaled, std::int64_t qty);

    // Wipes both sides. Used for `MdAction::Clear` (action == 'R').
    void clear() noexcept;

    // O(log) via std::map::begin / rbegin. nullopt-prices on empty side.
    Bbo bbo() const noexcept;

    // Returns total resting quantity at `price_scaled`; 0 if level is missing.
    std::int64_t volumeAtPrice(Side side, std::int64_t price_scaled) const noexcept;

    // Pretty-prints up to `depth` price levels per side. Format:
    //   bid[L]: <px> x <qty>   ask[L]: <px> x <qty>
    void printSnapshot(std::ostream& os, std::size_t depth = 5) const;

    std::size_t bidLevels() const noexcept { return bids_.size(); }
    std::size_t askLevels() const noexcept { return asks_.size(); }

  private:
    // bids: descending so begin() is the best bid; asks: ascending naturally.
    std::map<std::int64_t, std::int64_t, std::greater<>> bids_;
    std::map<std::int64_t, std::int64_t> asks_;
};

} // namespace cmf
