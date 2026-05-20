#pragma once

#include "OrderBook.hpp"
#include "PmrCompat.hpp"
#include <span>
#include <tuple>
#include <utility>

namespace cmf
{
class MapOrderBook : public OrderBook<MapOrderBook>
{
    friend class OrderBook<MapOrderBook>;

    using BidMap = PmrMap<ScaledPrice, ScaledPrice, std::greater<ScaledPrice>>;
    using AskMap = PmrMap<ScaledPrice, ScaledPrice>;
    using LevelPair = std::pair<ScaledPrice, ScaledPrice>;
    using OrderRecord =
        std::tuple<Side, ScaledPrice, uint32_t>; // (side, price, size)

#if CMF_HAS_STD_PMR
    MemoryResource* mr_;
#endif
    BidMap bids_;
    AskMap asks_;
    PmrUnorderedMap<uint64_t, OrderRecord> order_index_;
    mutable PmrVector<LevelPair> levels_cache_;

  public:
#if CMF_HAS_STD_PMR
    explicit MapOrderBook(MemoryResource* mr = default_memory_resource())
        : mr_(mr), bids_{mr_}, asks_{mr_}, order_index_{mr_}, levels_cache_{mr_}
    {
    }
#else
    MapOrderBook() = default;
#endif

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
