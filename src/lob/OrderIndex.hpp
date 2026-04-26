#pragma once

#include <cstdint>
#include <unordered_map>

namespace cmf {

struct OrderRecord {
    uint32_t instrument_id = 0;
    char     side          = 'N';
    int64_t  scaled_price  = 0;
    uint64_t remaining_qty = 0;
};

class OrderIndex {
public:
    explicit OrderIndex(std::size_t reserve = 1u << 20);

    void insert(uint64_t order_id, const OrderRecord& rec) noexcept;
    bool find(uint64_t order_id, OrderRecord& out) const noexcept;
    bool update_qty(uint64_t order_id, uint64_t new_qty) noexcept;
    void erase(uint64_t order_id) noexcept;

    std::size_t size() const noexcept { return map_.size(); }

private:
    std::unordered_map<uint64_t, OrderRecord> map_;
};

} // namespace cmf
