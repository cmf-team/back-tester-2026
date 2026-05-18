#include "LimitOrderBook.hpp"

#include <stdexcept>
#include <string>

namespace cmf
{

LimitOrderBook::~LimitOrderBook()
{
    clear_book();
}

bool LimitOrderBook::should_skip_event(const Flags flags) noexcept
{
    return has_flag(flags, Flags::BadTsRecv) || has_flag(flags, Flags::MaybeBadBook);
}

void LimitOrderBook::throw_unknown_order(const uint64_t order_id)
{
    throw std::runtime_error("LimitOrderBook: unknown order_id " +
                             std::to_string(order_id));
}

PriceLevel*& LimitOrderBook::tree_for(const Side side) noexcept
{
    return side == Side::Buy ? bid_tree_ : ask_tree_;
}

PriceLevel*& LimitOrderBook::best_for(const Side side) noexcept
{
    return side == Side::Buy ? best_bid_ : best_ask_;
}

LimitOrderBook::LevelMap& LimitOrderBook::levels_for(const Side side) noexcept
{
    return side == Side::Buy ? bid_levels_ : ask_levels_;
}

const LimitOrderBook::LevelMap& LimitOrderBook::levels_for(const Side side) const noexcept
{
    return side == Side::Buy ? bid_levels_ : ask_levels_;
}

PriceLevel* LimitOrderBook::tree_max(PriceLevel* n) noexcept
{
    while (n && n->right_child)
        n = n->right_child;
    return n;
}

PriceLevel* LimitOrderBook::tree_min(PriceLevel* n) noexcept
{
    while (n && n->left_child)
        n = n->left_child;
    return n;
}

const PriceLevel* LimitOrderBook::inorder_successor(const PriceLevel* n) noexcept
{
    if (n->right_child)
        return tree_min(const_cast<PriceLevel*>(n->right_child));
    while (n->parent && n->parent->right_child == n)
        n = n->parent;
    return n->parent;
}

const PriceLevel* LimitOrderBook::inorder_predecessor(const PriceLevel* n) noexcept
{
    if (n->left_child)
        return tree_max(const_cast<PriceLevel*>(n->left_child));
    while (n->parent && n->parent->left_child == n)
        n = n->parent;
    return n->parent;
}

void LimitOrderBook::free_tree(PriceLevel* n) noexcept
{
    if (!n)
        return;
    free_tree(n->left_child);
    free_tree(n->right_child);
    delete n;
}

void LimitOrderBook::apply_impl(const MarketDataEvent& e)
{
    if (should_skip_event(e.flags))
        return;

    switch (e.action)
    {
    case Action::Add:
        add_order(e);
        break;
    case Action::Cancel:
        cancel_order(e);
        break;
    case Action::Modify:
        modify_order(e);
        break;
    case Action::Clear:
        clear_book();
        break;
    case Action::Fill:
    case Action::Trade:
    case Action::None:
        break;
    }
}

std::optional<ScaledPrice> LimitOrderBook::best_price_impl(const Side side) const noexcept
{
    if (side == Side::Buy)
        return best_bid_ ? std::optional{best_bid_->price} : std::nullopt;
    if (side == Side::Sell)
        return best_ask_ ? std::optional{best_ask_->price} : std::nullopt;
    return std::nullopt;
}

uint64_t LimitOrderBook::volume_at_impl(const Side side,
                                        const ScaledPrice price) const noexcept
{
    if (side != Side::Buy && side != Side::Sell)
        return 0;
    const auto& levels = levels_for(side);
    const auto it = levels.find(price);
    return it != levels.end() ? static_cast<uint64_t>(it->second->total_quantity) : 0;
}

bool LimitOrderBook::empty_impl(const Side side) const noexcept
{
    switch (side)
    {
    case Side::Buy:
        return bid_tree_ == nullptr;
    case Side::Sell:
        return ask_tree_ == nullptr;
    default:
        return bid_tree_ == nullptr && ask_tree_ == nullptr;
    }
}

std::span<const LimitOrderBook::LevelPair>
LimitOrderBook::side_levels_impl(const Side side) const
{
    levels_cache_.clear();
    if (side == Side::Buy)
    {
        for (const PriceLevel* lv = best_bid_; lv; lv = inorder_predecessor(lv))
            levels_cache_.emplace_back(lv->price,
                                       static_cast<ScaledPrice>(lv->total_quantity));
    }
    else if (side == Side::Sell)
    {
        for (const PriceLevel* lv = best_ask_; lv; lv = inorder_successor(lv))
            levels_cache_.emplace_back(lv->price,
                                       static_cast<ScaledPrice>(lv->total_quantity));
    }
    return {levels_cache_};
}

void LimitOrderBook::clear_book()
{
    for (auto& [id, oe] : orders_)
        delete oe;
    orders_.clear();
    bid_levels_.clear();
    ask_levels_.clear();
    free_tree(bid_tree_);
    free_tree(ask_tree_);
    bid_tree_ = ask_tree_ = best_bid_ = best_ask_ = nullptr;
}

void LimitOrderBook::remove_level(PriceLevel* lv, const Side side)
{
    auto& tree = tree_for(side);
    tree = avl_remove(tree, lv);

    auto& best = best_for(side);
    if (lv == best)
        best = tree ? (side == Side::Buy ? tree_max(tree) : tree_min(tree)) : nullptr;

    levels_for(side).erase(lv->price);
    delete lv;
}

void LimitOrderBook::unlink_order(OrderEntry* oe)
{
    PriceLevel* lv = oe->level;
    if (oe->prev)
        oe->prev->next = oe->next;
    else
        lv->front = oe->next;
    if (oe->next)
        oe->next->prev = oe->prev;
    else
        lv->back = oe->prev;
}

void LimitOrderBook::add_order(const MarketDataEvent& ev)
{
    if (ev.side == Side::None || !ev.is_price_defined())
        return;

    const ScaledPrice sp = ev.price;
    const Side side = ev.side;

    PriceLevel* lv = nullptr;
    auto& levels = levels_for(side);
    const auto it = levels.find(sp);
    if (it == levels.end())
    {
        lv = new PriceLevel{};
        lv->price = sp;
        auto& tree = tree_for(side);
        tree = avl_insert(tree, lv);
        auto& best = best_for(side);
        if (side == Side::Buy)
        {
            if (!best || sp > best->price)
                best = lv;
        }
        else if (!best || sp < best->price)
        {
            best = lv;
        }
        levels[sp] = lv;
    }
    else
    {
        lv = it->second;
    }

    auto* oe = new OrderEntry{};
    oe->order_id = ev.order_id;
    oe->side = side;
    oe->quantity = static_cast<int64_t>(ev.size);
    oe->price = sp;
    oe->entry_time = ev.ts_event;
    oe->last_update = ev.ts_event;
    oe->level = lv;

    if (!lv->back)
        lv->front = lv->back = oe;
    else
    {
        oe->prev = lv->back;
        lv->back->next = oe;
        lv->back = oe;
    }

    lv->order_count++;
    lv->total_quantity += oe->quantity;
    orders_[ev.order_id] = oe;
}

void LimitOrderBook::cancel_order(const MarketDataEvent& ev)
{
    const auto it = orders_.find(ev.order_id);
    if (it == orders_.end())
        throw_unknown_order(ev.order_id);

    OrderEntry* oe = it->second;
    PriceLevel* lv = oe->level;
    const Side side = oe->side;
    const auto remaining = static_cast<int64_t>(ev.size);
    const int64_t delta = oe->quantity - remaining;

    if (delta > 0)
        lv->total_quantity -= delta;

    if (remaining == 0)
    {
        unlink_order(oe);
        lv->order_count--;
        orders_.erase(it);
        delete oe;
        if (lv->order_count == 0)
            remove_level(lv, side);
    }
    else
    {
        oe->quantity = remaining;
        oe->last_update = ev.ts_event;
    }
}

void LimitOrderBook::modify_order(const MarketDataEvent& ev)
{
    const auto it = orders_.find(ev.order_id);
    if (it == orders_.end())
        throw_unknown_order(ev.order_id);

    OrderEntry* oe = it->second;
    const ScaledPrice new_price = ev.is_price_defined() ? ev.price : oe->price;
    const Side new_side = ev.side != Side::None ? ev.side : oe->side;

    if (new_price == oe->price && new_side == oe->side)
    {
        const int64_t new_qty = static_cast<int64_t>(ev.size);
        oe->level->total_quantity += new_qty - oe->quantity;
        oe->quantity = new_qty;
        oe->last_update = ev.ts_event;
        return;
    }

    MarketDataEvent cancel_ev = ev;
    cancel_ev.size = 0;
    cancel_order(cancel_ev);

    MarketDataEvent add_ev = ev;
    add_ev.action = Action::Add;
    add_ev.side = new_side;
    add_ev.price = new_price;
    add_order(add_ev);
}

} // namespace cmf
