#pragma once

#include "OrderBook.hpp"
#include "OrderEntry.hpp"
#include "PriceLevel.hpp"
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cmf
{

class LimitOrderBook : public OrderBook<LimitOrderBook>
{
    friend class OrderBook<LimitOrderBook>;

    using LevelPair = std::pair<ScaledPrice, ScaledPrice>;
    using LevelMap = std::unordered_map<ScaledPrice, PriceLevel*>;

  public:
    LimitOrderBook() = default;
    ~LimitOrderBook();

    LimitOrderBook(const LimitOrderBook&) = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;
    LimitOrderBook(LimitOrderBook&&) = default;
    LimitOrderBook& operator=(LimitOrderBook&&) = default;

  protected:
    void apply_impl(const MarketDataEvent& event);

    [[nodiscard]] std::optional<ScaledPrice>
    best_price_impl(Side side) const noexcept;

    [[nodiscard]] uint64_t volume_at_impl(Side side,
                                          ScaledPrice price) const noexcept;

    [[nodiscard]] bool empty_impl(Side side) const noexcept;

    [[nodiscard]] std::span<const LevelPair> side_levels_impl(Side side) const;

  private:
    PriceLevel* bid_tree_ = nullptr;
    PriceLevel* ask_tree_ = nullptr;
    PriceLevel* best_bid_ = nullptr;
    PriceLevel* best_ask_ = nullptr;

    LevelMap bid_levels_;
    LevelMap ask_levels_;
    std::unordered_map<uint64_t, OrderEntry*> orders_;

    mutable std::vector<LevelPair> levels_cache_;

    [[nodiscard]] static bool should_skip_event(Flags flags) noexcept;
    [[noreturn]] static void throw_unknown_order(uint64_t order_id);

    void add_order(const MarketDataEvent& ev);
    void cancel_order(const MarketDataEvent& ev);
    void modify_order(const MarketDataEvent& ev);
    void clear_book();

    void remove_level(PriceLevel* lv, Side side);

    [[nodiscard]] PriceLevel*& tree_for(Side side) noexcept;
    [[nodiscard]] PriceLevel*& best_for(Side side) noexcept;
    [[nodiscard]] LevelMap& levels_for(Side side) noexcept;
    [[nodiscard]] const LevelMap& levels_for(Side side) const noexcept;

    static PriceLevel* tree_max(PriceLevel* n) noexcept;
    static PriceLevel* tree_min(PriceLevel* n) noexcept;
    static const PriceLevel* inorder_successor(const PriceLevel* n) noexcept;
    static const PriceLevel* inorder_predecessor(const PriceLevel* n) noexcept;
    static void free_tree(PriceLevel* n) noexcept;

    void unlink_order(OrderEntry* oe);
};

} // namespace cmf
