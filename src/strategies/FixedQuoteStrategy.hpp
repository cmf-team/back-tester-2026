#pragma once

#include "strategies/Strategy.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace md {

struct FixedQuoteStrategyConfig {
    std::uint64_t instrument_id{};
    std::uint64_t order_size{1};
    std::int64_t quote_offset_ticks{1};
    std::int64_t tick_size{1};
    std::uint64_t quote_interval_events{1};
    std::int64_t max_inventory{100};
};

class FixedQuoteStrategy final : public Strategy {
public:
    explicit FixedQuoteStrategy(FixedQuoteStrategyConfig config);

    [[nodiscard]] std::string name() const override;

    std::vector<StrategyAction> onMarketData(
        const MarketDataEvent& event,
        const MarketView& market,
        const Portfolio& portfolio,
        const OrderManager& orders
    ) override;

private:
    [[nodiscard]] bool shouldQuote(const MarketDataEvent& event, const MarketView& market);
    [[nodiscard]] std::int64_t inventory(const Portfolio& portfolio) const;
    [[nodiscard]] std::int64_t quoteOffset() const;
    [[nodiscard]] bool canPlaceBuy(std::int64_t current_inventory) const;
    [[nodiscard]] bool canPlaceSell(std::int64_t current_inventory) const;
    void appendCancelActions(const OrderManager& orders, std::vector<StrategyAction>& actions) const;

    FixedQuoteStrategyConfig config_;
    std::uint64_t eligible_event_count_{};
};

} // namespace md
