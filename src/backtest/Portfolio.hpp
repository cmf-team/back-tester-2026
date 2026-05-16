#pragma once

#include "backtest/ExecutionSimulator.hpp"

#include <cstdint>
#include <unordered_map>

namespace md {

struct InstrumentPosition {
    std::int64_t inventory{};
    long double cash{};
    long double turnover{};
    std::uint64_t fills{};
};

class Portfolio {
public:
    explicit Portfolio(long double multiplier = 1.0L);

    void applyFill(const Fill& fill);

    [[nodiscard]] const InstrumentPosition* findPosition(std::uint64_t instrument_id) const;
    [[nodiscard]] const std::unordered_map<std::uint64_t, InstrumentPosition>& positions() const noexcept;
    [[nodiscard]] long double multiplier() const noexcept;

private:
    long double multiplier_{1.0L};
    std::unordered_map<std::uint64_t, InstrumentPosition> positions_by_instrument_;
};

} // namespace md
