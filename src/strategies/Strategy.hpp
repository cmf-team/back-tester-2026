#pragma once

#include "backtest/MarketView.hpp"
#include "backtest/OrderManager.hpp"
#include "backtest/Portfolio.hpp"
#include "backtest/StrategyAction.hpp"
#include "domain/MarketDataEvent.hpp"

#include <string>
#include <vector>

namespace md {

class Strategy {
public:
    virtual ~Strategy() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    virtual std::vector<StrategyAction> onMarketData(
        const MarketDataEvent& event,
        const MarketView& market,
        const Portfolio& portfolio,
        const OrderManager& orders
    ) = 0;
};

} // namespace md
