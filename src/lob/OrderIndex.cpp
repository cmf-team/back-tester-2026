#include "lob/OrderIndex.hpp"

namespace cmf {

OrderIndex::OrderIndex(std::size_t reserve) {
    map_.reserve(reserve);
}

void OrderIndex::insert(uint64_t order_id, const OrderRecord& rec) noexcept {
    map_[order_id] = rec;
}

bool OrderIndex::find(uint64_t order_id, OrderRecord& out) const noexcept {
    auto it = map_.find(order_id);
    if (it == map_.end()) return false;
    out = it->second;
    return true;
}

bool OrderIndex::update_qty(uint64_t order_id, uint64_t new_qty) noexcept {
    auto it = map_.find(order_id);
    if (it == map_.end()) return false;
    it->second.remaining_qty = new_qty;
    return true;
}

void OrderIndex::erase(uint64_t order_id) noexcept {
    map_.erase(order_id);
}

} // namespace cmf
