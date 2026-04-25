#pragma once

#include "OrderEntry.hpp"
#include <cstdint>

namespace cmf {

struct PriceLevel {
    int64_t price          = 0;
    int64_t order_count    = 0;
    int64_t total_quantity = 0;

    // AVL BST links
    PriceLevel* parent      = nullptr;
    PriceLevel* left_child  = nullptr;
    PriceLevel* right_child = nullptr;
    int         height      = 1;

    // FIFO queue of resting orders at this price
    OrderEntry* front = nullptr;  // oldest (executes first)
    OrderEntry* back  = nullptr;  // newest (appended here)
};

// AVL operations — all return the new root of the subtree
PriceLevel* avl_insert(PriceLevel* root, PriceLevel* node);
PriceLevel* avl_remove(PriceLevel* root, PriceLevel* node);

} // namespace cmf
