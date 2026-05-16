#include "backtest/Portfolio.hpp"

namespace md {
namespace {

long double fillNotional(const Fill& fill, long double multiplier) {
    return static_cast<long double>(fill.price) * static_cast<long double>(fill.quantity) * multiplier;
}

std::int64_t signedQuantity(std::uint64_t quantity) {
    return static_cast<std::int64_t>(quantity);
}

} // namespace

Portfolio::Portfolio(long double multiplier)
    : multiplier_(multiplier) {}

void Portfolio::applyFill(const Fill& fill) {
    auto& position = positions_by_instrument_[fill.instrument_id];
    const auto notional = fillNotional(fill, multiplier_);

    switch (fill.side) {
        case SimOrderSide::Buy:
            position.inventory += signedQuantity(fill.quantity);
            position.cash -= notional;
            break;
        case SimOrderSide::Sell:
            position.inventory -= signedQuantity(fill.quantity);
            position.cash += notional;
            break;
    }

    position.turnover += notional;
    ++position.fills;
}

const InstrumentPosition* Portfolio::findPosition(std::uint64_t instrument_id) const {
    const auto iter = positions_by_instrument_.find(instrument_id);
    if (iter == positions_by_instrument_.end()) {
        return nullptr;
    }
    return &iter->second;
}

const std::unordered_map<std::uint64_t, InstrumentPosition>& Portfolio::positions() const noexcept {
    return positions_by_instrument_;
}

long double Portfolio::multiplier() const noexcept {
    return multiplier_;
}

} // namespace md
