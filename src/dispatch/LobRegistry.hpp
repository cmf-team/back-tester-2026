#pragma once

#include "lob/LimitOrderBook.hpp"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace cmf {

class LobRegistry {
public:
    LimitOrderBook& get_or_create(uint32_t instrument_id) {
        auto it = books_.find(instrument_id);
        if (it != books_.end()) return *it->second;
        auto [ins, _] = books_.emplace(instrument_id,
                                       std::make_unique<LimitOrderBook>(instrument_id));
        return *ins->second;
    }

    LimitOrderBook* try_get(uint32_t instrument_id) noexcept {
        auto it = books_.find(instrument_id);
        return it == books_.end() ? nullptr : it->second.get();
    }

    std::size_t size() const noexcept { return books_.size(); }

    template <class Fn>
    void for_each(Fn&& fn) const {
        for (auto& [id, book] : books_) fn(id, *book);
    }

private:
    std::unordered_map<uint32_t, std::unique_ptr<LimitOrderBook>> books_;
};

} // namespace cmf
