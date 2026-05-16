#pragma once

#include "strategies/Strategy.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace md {

struct MicropriceAvellanedaStoikovConfig {
    std::uint64_t instrument_id{};
    std::uint64_t order_size{1};

    long double gamma{};
    long double sigma{};
    long double k{};
    long double horizon_seconds{};
    std::int64_t tick_size{};
    std::int64_t max_inventory{};

    std::size_t quote_interval_events{100};
    bool use_imbalance_skew{false};
    long double imbalance_alpha_ticks{};
};

class MicropriceAvellanedaStoikovStrategy final : public Strategy {
public:
    explicit MicropriceAvellanedaStoikovStrategy(MicropriceAvellanedaStoikovConfig config);

    [[nodiscard]] std::string name() const override;

    std::vector<StrategyAction> onMarketData(
        const MarketDataEvent& event,
        const MarketView& market,
        const Portfolio& portfolio,
        const OrderManager& orders
    ) override;

    [[nodiscard]] long double fairPrice(const MarketView& market) const;
    [[nodiscard]] long double reservationPrice(long double fair_price, std::int64_t inventory) const;
    [[nodiscard]] long double optimalSpread() const;
    [[nodiscard]] long double imbalance(const MarketView& market) const;

private:
    [[nodiscard]] bool shouldQuote(const MarketDataEvent& event, const MarketView& market);
    [[nodiscard]] bool hasValidParameters() const;
    [[nodiscard]] std::int64_t inventory(const Portfolio& portfolio) const;
    [[nodiscard]] bool canPlaceBuy(std::int64_t current_inventory) const;
    [[nodiscard]] bool canPlaceSell(std::int64_t current_inventory) const;
    [[nodiscard]] std::int64_t roundDownToTick(long double price) const;
    [[nodiscard]] std::int64_t roundUpToTick(long double price) const;
    void appendCancelActions(const OrderManager& orders, std::vector<StrategyAction>& actions) const;

    MicropriceAvellanedaStoikovConfig config_;
    std::size_t eligible_event_count_{};
};

} // namespace md
