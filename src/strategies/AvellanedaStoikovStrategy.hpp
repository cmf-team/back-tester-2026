#pragma once

#include "strategies/Strategy.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace md {

struct AvellanedaStoikovConfig {
    std::uint64_t instrument_id{};
    std::uint64_t order_size{1};

    long double gamma{};
    long double sigma{};
    long double k{};
    long double horizon_seconds{};
    std::int64_t tick_size{};
    std::int64_t max_inventory{};

    std::size_t quote_interval_events{100};
};

class AvellanedaStoikovStrategy final : public Strategy {
public:
    explicit AvellanedaStoikovStrategy(AvellanedaStoikovConfig config);

    [[nodiscard]] std::string name() const override;

    std::vector<StrategyAction> onMarketData(
        const MarketDataEvent& event,
        const MarketView& market,
        const Portfolio& portfolio,
        const OrderManager& orders
    ) override;

    [[nodiscard]] long double reservationPrice(std::int64_t mid_price, std::int64_t inventory) const;
    [[nodiscard]] long double optimalSpread() const;

private:
    [[nodiscard]] bool shouldQuote(const MarketDataEvent& event, const MarketView& market);
    [[nodiscard]] bool hasValidParameters() const;
    [[nodiscard]] std::int64_t inventory(const Portfolio& portfolio) const;
    [[nodiscard]] bool canPlaceBuy(std::int64_t current_inventory) const;
    [[nodiscard]] bool canPlaceSell(std::int64_t current_inventory) const;
    [[nodiscard]] std::int64_t roundDownToTick(long double price) const;
    [[nodiscard]] std::int64_t roundUpToTick(long double price) const;
    void appendCancelActions(const OrderManager& orders, std::vector<StrategyAction>& actions) const;

    AvellanedaStoikovConfig config_;
    std::size_t eligible_event_count_{};
};

} // namespace md
