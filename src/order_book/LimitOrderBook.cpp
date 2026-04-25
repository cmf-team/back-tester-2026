#include "order_book/LimitOrderBook.hpp"

#include "market_data/MarketDataEvent.hpp"

#include <cstdint>
#include <ios>
#include <ostream>
#include <stdexcept>

namespace cmf
{

namespace
{

// Adjusts a price level by `delta`. Erases the level when the aggregate
// drops to zero or below — keeps the map shrinking as orders are cancelled.
template <typename Map>
void adjust_level(Map& levels, std::int64_t price, std::int64_t delta)
{
    auto it = levels.find(price);
    if (it == levels.end())
    {
        if (delta > 0)
            levels.emplace(price, delta);
        return;
    }
    it->second += delta;
    if (it->second <= 0)
        levels.erase(it);
}

double scaledToDouble(std::int64_t scaled)
{
    return static_cast<double>(scaled) /
           static_cast<double>(MarketDataEvent::kPriceScale);
}

} // namespace

void LimitOrderBook::applyDelta(Side side, std::int64_t price_scaled,
                                std::int64_t qty)
{
    if (qty == 0)
        return;
    if (price_scaled == MarketDataEvent::kUndefPrice)
        return; // ignore deltas with undefined price (e.g. trade summary lines)
    switch (side)
    {
    case Side::Buy:
        adjust_level(bids_, price_scaled, qty);
        return;
    case Side::Sell:
        adjust_level(asks_, price_scaled, qty);
        return;
    case Side::None:
        throw std::invalid_argument(
            "LimitOrderBook::applyDelta: Side::None is not allowed");
    }
}

void LimitOrderBook::clear() noexcept
{
    bids_.clear();
    asks_.clear();
}

LimitOrderBook::Bbo LimitOrderBook::bbo() const noexcept
{
    Bbo out;
    if (!bids_.empty())
    {
        const auto it = bids_.begin();
        out.bid_price = it->first;
        out.bid_size = it->second;
    }
    if (!asks_.empty())
    {
        const auto it = asks_.begin();
        out.ask_price = it->first;
        out.ask_size = it->second;
    }
    return out;
}

std::int64_t
LimitOrderBook::volumeAtPrice(Side side, std::int64_t price_scaled) const noexcept
{
    if (side == Side::Buy)
    {
        const auto it = bids_.find(price_scaled);
        return it == bids_.end() ? 0 : it->second;
    }
    if (side == Side::Sell)
    {
        const auto it = asks_.find(price_scaled);
        return it == asks_.end() ? 0 : it->second;
    }
    return 0;
}

void LimitOrderBook::printSnapshot(std::ostream& os, std::size_t depth) const
{
    const auto flags = os.flags();
    os.setf(std::ios::fixed, std::ios::floatfield);
    const auto prec = os.precision(9);

    std::size_t lvl = 0;
    for (auto it = bids_.begin(); it != bids_.end() && lvl < depth; ++it, ++lvl)
    {
        os << "  bid[L" << (lvl + 1) << "] " << scaledToDouble(it->first) << " x "
           << it->second << "\n";
    }
    lvl = 0;
    for (auto it = asks_.begin(); it != asks_.end() && lvl < depth; ++it, ++lvl)
    {
        os << "  ask[L" << (lvl + 1) << "] " << scaledToDouble(it->first) << " x "
           << it->second << "\n";
    }

    os.flags(flags);
    os.precision(prec);
}

} // namespace cmf
