#pragma once

#include "OrderBook.hpp"
#include <absl/container/btree_map.h>
#include <span>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cmf
{
class AbseilOrderBook : public OrderBook<AbseilOrderBook>
{
    friend class OrderBook<AbseilOrderBook>;

    using BidMap = absl::btree_map<int64_t, int64_t, std::greater<int64_t>>;
    using AskMap = absl::btree_map<int64_t, int64_t>;
    using LevelPair = std::pair<int64_t, int64_t>;
    using OrderRecord =
        std::tuple<Side, int64_t, uint32_t>; // (side, price, size)

    BidMap bids_;
    AskMap asks_;
    std::unordered_map<uint64_t, OrderRecord> order_index_;
    mutable std::vector<LevelPair> levels_cache_;

  public:
    AbseilOrderBook() = default;

  protected:
    void apply_impl(const MarketDataEvent& event);

    [[nodiscard]] std::optional<int64_t>
    best_price_impl(Side side) const noexcept;

    [[nodiscard]] uint64_t volume_at_impl(Side side,
                                          int64_t price) const noexcept;

    [[nodiscard]] bool empty_impl(Side side) const noexcept;

    [[nodiscard]] std::span<const LevelPair> side_levels_impl(Side side) const;

  private:
    void add_to_level(Side side, int64_t price, int64_t delta);
    void remove_from_level(Side side, int64_t price, int64_t delta);
};
} // namespace cmf
