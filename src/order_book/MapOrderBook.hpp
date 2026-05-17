#pragma once

#include "OrderBook.hpp"
#include <map>
#include <memory_resource>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace cmf
{
class MapOrderBook : public OrderBook<MapOrderBook>
{
    friend class OrderBook<MapOrderBook>;

    using BidMap = std::pmr::map<ScaledPrice, ScaledPrice, std::greater<ScaledPrice>>;
    using AskMap = std::pmr::map<ScaledPrice, ScaledPrice>;
    using LevelPair = std::pair<ScaledPrice, ScaledPrice>;
    using OrderRecord =
        std::tuple<Side, ScaledPrice, uint32_t>; // (side, price, size)

    std::pmr::memory_resource* mr_;
    BidMap bids_;
    AskMap asks_;
    std::pmr::unordered_map<uint64_t, OrderRecord> order_index_;
    mutable std::pmr::vector<LevelPair> levels_cache_;

  public:
    explicit MapOrderBook(
        std::pmr::memory_resource* mr = std::pmr::get_default_resource())
        : mr_(mr), bids_{mr_}, asks_{mr_}, order_index_{mr_}, levels_cache_{mr_}
    {
    }

  protected:
    void apply_impl(const MarketDataEvent& event);

    [[nodiscard]] std::optional<ScaledPrice>
    best_price_impl(Side side) const noexcept;

    [[nodiscard]] uint64_t volume_at_impl(Side side,
                                          ScaledPrice price) const noexcept;

    [[nodiscard]] bool empty_impl(Side side) const noexcept;

    [[nodiscard]] std::span<const LevelPair> side_levels_impl(Side side) const;

  private:
    void add_to_level(Side side, ScaledPrice price, ScaledPrice delta);
    void remove_from_level(Side side, ScaledPrice price, ScaledPrice delta);
};
} // namespace cmf
