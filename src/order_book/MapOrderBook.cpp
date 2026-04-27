#include "MapOrderBook.hpp"

namespace cmf
{

void MapOrderBook::add_to_level(Side side, int64_t price, int64_t delta)
{
    auto& level = side == Side::Buy ? bids_[price] : asks_[price];
    level += delta;
}

void MapOrderBook::remove_from_level(Side side, int64_t price, int64_t delta)
{
    if (side == Side::Buy)
    {
        auto it = bids_.find(price);
        if (it == bids_.end())
            return;
        it->second -= delta;
        if (it->second <= 0)
            bids_.erase(it);
    }
    else
    {
        auto it = asks_.find(price);
        if (it == asks_.end())
            return;
        it->second -= delta;
        if (it->second <= 0)
            asks_.erase(it);
    }
}

void MapOrderBook::apply_impl(const MarketDataEvent& e)
{
    switch (e.action)
    {
    case Action::Add:
    {
        if (e.side == Side::None || !e.is_price_defined())
            break;
        add_to_level(e.side, e.price, static_cast<int64_t>(e.size));
        order_index_[e.order_id] = {e.side, e.price, e.size};
        break;
    }
    case Action::Cancel:
    {
        auto it = order_index_.find(e.order_id);
        if (it == order_index_.end())
            break;
        auto& [side, price, stored_size] = it->second;
        auto remaining = static_cast<int64_t>(e.size);
        int64_t delta = static_cast<int64_t>(stored_size) - remaining;
        if (delta > 0)
            remove_from_level(side, price, delta);
        if (remaining == 0)
            order_index_.erase(it);
        else
            stored_size = static_cast<uint32_t>(remaining);
        break;
    }
    case Action::Fill:
        // Resting order was filled — does not affect the book
        break;
    case Action::Modify:
    {
        auto it = order_index_.find(e.order_id);
        if (it == order_index_.end())
        {
            // Treat as Add if not tracked yet
            if (e.side != Side::None && e.is_price_defined())
            {
                add_to_level(e.side, e.price, static_cast<int64_t>(e.size));
                order_index_[e.order_id] = {e.side, e.price, e.size};
            }
            break;
        }
        auto& [old_side, old_price, old_size] = it->second;
        remove_from_level(old_side, old_price, static_cast<int64_t>(old_size));
        Side new_side = (e.side != Side::None) ? e.side : old_side;
        int64_t new_price = e.is_price_defined() ? e.price : old_price;
        add_to_level(new_side, new_price, static_cast<int64_t>(e.size));
        it->second = {new_side, new_price, e.size};
        break;
    }
    case Action::Trade:
        // Aggressing order — does not affect the resting book
        break;
    case Action::Clear:
        bids_.clear();
        asks_.clear();
        order_index_.clear();
        break;
    case Action::None:
        break;
    }
}

std::optional<int64_t> MapOrderBook::best_price_impl(Side side) const noexcept
{
    if (side == Side::Buy)
        return bids_.empty() ? std::nullopt : std::optional{bids_.begin()->first};
    if (side == Side::Sell)
        return asks_.empty() ? std::nullopt : std::optional{asks_.begin()->first};
    return std::nullopt;
}

uint64_t MapOrderBook::volume_at_impl(Side side, int64_t price) const noexcept
{
    if (side == Side::Buy)
    {
        auto it = bids_.find(price);
        return it != bids_.end() ? static_cast<uint64_t>(it->second) : 0;
    }
    if (side == Side::Sell)
    {
        auto it = asks_.find(price);
        return it != asks_.end() ? static_cast<uint64_t>(it->second) : 0;
    }
    return 0;
}

bool MapOrderBook::empty_impl(Side side) const noexcept
{
    switch (side)
    {
    case Side::Buy:
        return bids_.empty();
    case Side::Sell:
        return asks_.empty();
    default:
        return asks_.empty() && bids_.empty();
    }
}

std::span<const MapOrderBook::LevelPair>
MapOrderBook::side_levels_impl(Side side) const
{
    levels_cache_.clear();
    if (side == Side::Buy)
    {
        for (const auto& [price, qty] : bids_)
            levels_cache_.emplace_back(price, qty);
    }
    else
    {
        for (const auto& [price, qty] : asks_)
            levels_cache_.emplace_back(price, qty);
    }
    return {levels_cache_};
}

} // namespace cmf
