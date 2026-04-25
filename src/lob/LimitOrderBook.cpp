#include "LimitOrderBook.hpp"
#include "common/enums.hpp"
#include <cstdio>

namespace cmf {

// ---------------------------------------------------------------------------
// Tree-walking helpers (non-member)
// ---------------------------------------------------------------------------

static PriceLevel* tree_max(PriceLevel* n) {
    while (n && n->right_child) n = n->right_child;
    return n;
}

static PriceLevel* tree_min(PriceLevel* n) {
    while (n && n->left_child) n = n->left_child;
    return n;
}

static const PriceLevel* inorder_successor(const PriceLevel* n) {
    if (n->right_child) return tree_min(const_cast<PriceLevel*>(n->right_child));
    while (n->parent && n->parent->right_child == n) n = n->parent;
    return n->parent;
}

static const PriceLevel* inorder_predecessor(const PriceLevel* n) {
    if (n->left_child) return tree_max(const_cast<PriceLevel*>(n->left_child));
    while (n->parent && n->parent->left_child == n) n = n->parent;
    return n->parent;
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

LimitOrderBook::~LimitOrderBook() { clear(); }

void LimitOrderBook::apply(const MarketDataEvent& ev) {
    if (flags::should_skip(ev.flags)) return;

    switch (ev.action) {
        case action::Action::Add:    add_order(ev);    break;
        case action::Action::Cancel: cancel_order(ev); break;
        case action::Action::Modify: modify_order(ev); break;
        case action::Action::Clear:  clear();           break;
        default: break;  // Trade, Fill, None — no resting book change
    }
}

int64_t LimitOrderBook::volume_at(int64_t scaled_price) const {
    auto it = levels_.find(scaled_price);
    return it != levels_.end() ? it->second->total_quantity : 0;
}

void LimitOrderBook::print_snapshot(int depth) const {
    // Collect ask levels in ascending order (best_ask = min), then print in reverse
    const PriceLevel* lv = best_ask;
    int ask_count = 0;
    const PriceLevel* ask_levels[64];
    while (lv && ask_count < depth && ask_count < 64) {
        ask_levels[ask_count++] = lv;
        lv = inorder_successor(lv);
    }
    for (int i = ask_count - 1; i >= 0; --i) {
        std::printf("  ASK  %.9f  qty=%lld  orders=%lld\n",
            unscale_price(ask_levels[i]->price),
            static_cast<long long>(ask_levels[i]->total_quantity),
            static_cast<long long>(ask_levels[i]->order_count));
    }

    std::printf("  ---\n");

    // Bid levels in descending order (best_bid = max)
    lv = best_bid;
    for (int i = 0; i < depth && lv; ++i) {
        std::printf("  BID  %.9f  qty=%lld  orders=%lld\n",
            unscale_price(lv->price),
            static_cast<long long>(lv->total_quantity),
            static_cast<long long>(lv->order_count));
        lv = inorder_predecessor(lv);
    }
}

void LimitOrderBook::clear() {
    for (auto& [id, oe] : orders_) delete oe;
    orders_.clear();
    for (auto& [price, lv] : levels_) delete lv;
    levels_.clear();
    bid_tree = ask_tree = best_bid = best_ask = nullptr;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void LimitOrderBook::free_tree(PriceLevel* n) {
    if (!n) return;
    free_tree(n->left_child);
    free_tree(n->right_child);
    delete n;
}

void LimitOrderBook::remove_level(PriceLevel* lv, side::Side s) {
    if (s == side::Side::Buy) {
        bid_tree = avl_remove(bid_tree, lv);
        if (lv == best_bid)
            best_bid = bid_tree ? tree_max(bid_tree) : nullptr;
    } else {
        ask_tree = avl_remove(ask_tree, lv);
        if (lv == best_ask)
            best_ask = ask_tree ? tree_min(ask_tree) : nullptr;
    }
    levels_.erase(lv->price);
    delete lv;
}

void LimitOrderBook::add_order(const MarketDataEvent& ev) {
    if (ev.side == side::Side::None) return;

    const int64_t sp = scale_price(ev.price);

    PriceLevel* lv = nullptr;
    auto it = levels_.find(sp);
    if (it == levels_.end()) {
        lv = new PriceLevel{};
        lv->price = sp;
        if (ev.side == side::Side::Buy) {
            bid_tree = avl_insert(bid_tree, lv);
            if (!best_bid || sp > best_bid->price) best_bid = lv;
        } else {
            ask_tree = avl_insert(ask_tree, lv);
            if (!best_ask || sp < best_ask->price) best_ask = lv;
        }
        levels_[sp] = lv;
    } else {
        lv = it->second;
    }

    OrderEntry* oe  = new OrderEntry{};
    oe->order_id    = ev.order_id;
    oe->side        = ev.side;
    oe->quantity    = static_cast<int64_t>(ev.qty);
    oe->price       = sp;
    oe->entry_time  = ev.ts_event;
    oe->last_update = ev.ts_event;
    oe->level       = lv;

    // Append at tail (FIFO)
    if (!lv->back) {
        lv->front = lv->back = oe;
    } else {
        oe->prev      = lv->back;
        lv->back->next = oe;
        lv->back       = oe;
    }

    lv->order_count++;
    lv->total_quantity += oe->quantity;
    orders_[ev.order_id] = oe;
}

void LimitOrderBook::cancel_order(const MarketDataEvent& ev) {
    auto it = orders_.find(ev.order_id);
    if (it == orders_.end()) return;

    OrderEntry* oe  = it->second;
    PriceLevel* lv  = oe->level;
    side::Side  s   = oe->side;
    int64_t     qty = oe->quantity;

    // Unlink from doubly-linked FIFO list
    if (oe->prev) oe->prev->next = oe->next; else lv->front = oe->next;
    if (oe->next) oe->next->prev = oe->prev; else lv->back  = oe->prev;

    lv->order_count--;
    lv->total_quantity -= qty;

    orders_.erase(it);
    delete oe;

    if (lv->order_count == 0)
        remove_level(lv, s);
}

void LimitOrderBook::modify_order(const MarketDataEvent& ev) {
    auto it = orders_.find(ev.order_id);
    if (it == orders_.end()) return;

    OrderEntry* oe   = it->second;
    const int64_t new_sp = scale_price(ev.price);

    if (new_sp == oe->price) {
        // Price unchanged — update quantity in place
        const int64_t new_qty = static_cast<int64_t>(ev.qty);
        oe->level->total_quantity += new_qty - oe->quantity;
        oe->quantity    = new_qty;
        oe->last_update = ev.ts_event;
    } else {
        // Price changed — cancel then re-add
        cancel_order(ev);
        add_order(ev);
    }
}

} // namespace cmf
