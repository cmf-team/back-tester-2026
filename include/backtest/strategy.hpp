#pragma once

#include "backtest/csv_data_loader.hpp"
#include <concepts>

namespace backtest {

// Определяет интерфейс, который должна реализовывать стратегия
template<typename T>
concept Strategy = requires(T typ, const MarketEvent& event) {
    { typ.on_init() } -> std::same_as<void>;
    { typ.on_event(event) } -> std::same_as<void>;
    { typ.on_finish() } -> std::same_as<void>;
};

struct BasicStrategy {
    void on_init() noexcept {}
    void on_event(const MarketEvent& /*ev*/) noexcept {}
    void on_finish() noexcept {}
};

}