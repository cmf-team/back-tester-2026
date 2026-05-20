#pragma once

#include "OrderEntry.hpp"
#include <cstdint>

namespace cmf
{

struct PriceLevel
{
    ScaledPrice price = 0;
    int64_t order_count = 0;
    int64_t total_quantity = 0;

    PriceLevel* parent = nullptr;
    PriceLevel* left_child = nullptr;
    PriceLevel* right_child = nullptr;
    int height = 1;

    OrderEntry* front = nullptr;
    OrderEntry* back = nullptr;
};

PriceLevel* avl_insert(PriceLevel* root, PriceLevel* node);
PriceLevel* avl_remove(PriceLevel* root, PriceLevel* node);

} // namespace cmf
