#include "strategies/AvellanedaStoikovStrategy.hpp"

#include <algorithm>
#include <cmath>

namespace md {
namespace {

std::size_t normalizedInterval(std::size_t interval) {
    return std::max<std::size_t>(interval, 1);
}

std::int64_t quantityAsSigned(std::uint64_t quantity) {
    return static_cast<std::int64_t>(quantity);
}

} // namespace

AvellanedaStoikovStrategy::AvellanedaStoikovStrategy(AvellanedaStoikovConfig config)
    : config_(config) {}

std::string AvellanedaStoikovStrategy::name() const {
    return "avellaneda_stoikov";
}

std::vector<StrategyAction> AvellanedaStoikovStrategy::onMarketData(
    const MarketDataEvent& event,
    const MarketView& market,
    const Portfolio& portfolio,
    const OrderManager& orders
) {
    std::vector<StrategyAction> actions;
    if (!shouldQuote(event, market)) {
        return actions;
    }

    appendCancelActions(orders, actions);

    const auto current_inventory = inventory(portfolio);
    const auto reservation_price = reservationPrice(*market.mid_price, current_inventory);
    const auto half_spread = optimalSpread() / 2.0L;

    auto bid_price = roundDownToTick(reservation_price - half_spread);
    auto ask_price = roundUpToTick(reservation_price + half_spread);
    if (bid_price >= ask_price) {
        ask_price = bid_price + config_.tick_size;
    }

    if (canPlaceBuy(current_inventory)) {
        actions.push_back(PlaceOrderAction{
            .instrument_id = config_.instrument_id,
            .side = SimOrderSide::Buy,
            .price = bid_price,
            .quantity = config_.order_size
        });
    }

    if (canPlaceSell(current_inventory)) {
        actions.push_back(PlaceOrderAction{
            .instrument_id = config_.instrument_id,
            .side = SimOrderSide::Sell,
            .price = ask_price,
            .quantity = config_.order_size
        });
    }

    return actions;
}

long double AvellanedaStoikovStrategy::reservationPrice(
    std::int64_t mid_price,
    std::int64_t inventory
) const {
    const auto variance = config_.sigma * config_.sigma;
    return static_cast<long double>(mid_price)
        - static_cast<long double>(inventory) * config_.gamma * variance * config_.horizon_seconds;
}

long double AvellanedaStoikovStrategy::optimalSpread() const {
    const auto variance = config_.sigma * config_.sigma;
    return config_.gamma * variance * config_.horizon_seconds
        + (2.0L / config_.gamma) * std::log(1.0L + config_.gamma / config_.k);
}

bool AvellanedaStoikovStrategy::shouldQuote(const MarketDataEvent& event, const MarketView& market) {
    if (!hasValidParameters()
        || event.instrument_id != config_.instrument_id
        || market.instrument_id != config_.instrument_id
        || !market.mid_price.has_value()
        || config_.order_size == 0) {
        return false;
    }

    ++eligible_event_count_;
    const auto interval = normalizedInterval(config_.quote_interval_events);
    return (eligible_event_count_ - 1) % interval == 0;
}

bool AvellanedaStoikovStrategy::hasValidParameters() const {
    return config_.gamma > 0.0L
        && config_.sigma >= 0.0L
        && config_.k > 0.0L
        && config_.horizon_seconds >= 0.0L
        && config_.tick_size > 0
        && config_.max_inventory >= 0;
}

std::int64_t AvellanedaStoikovStrategy::inventory(const Portfolio& portfolio) const {
    const auto* position = portfolio.findPosition(config_.instrument_id);
    return position == nullptr ? 0 : position->inventory;
}

bool AvellanedaStoikovStrategy::canPlaceBuy(std::int64_t current_inventory) const {
    return current_inventory + quantityAsSigned(config_.order_size) <= config_.max_inventory;
}

bool AvellanedaStoikovStrategy::canPlaceSell(std::int64_t current_inventory) const {
    return current_inventory - quantityAsSigned(config_.order_size) >= -config_.max_inventory;
}

std::int64_t AvellanedaStoikovStrategy::roundDownToTick(long double price) const {
    const auto ticks = std::floor(price / static_cast<long double>(config_.tick_size));
    return static_cast<std::int64_t>(ticks) * config_.tick_size;
}

std::int64_t AvellanedaStoikovStrategy::roundUpToTick(long double price) const {
    const auto ticks = std::ceil(price / static_cast<long double>(config_.tick_size));
    return static_cast<std::int64_t>(ticks) * config_.tick_size;
}

void AvellanedaStoikovStrategy::appendCancelActions(
    const OrderManager& orders,
    std::vector<StrategyAction>& actions
) const {
    for (const auto* order : orders.liveOrdersForInstrument(config_.instrument_id)) {
        actions.push_back(CancelOrderAction{
            .client_order_id = order->client_order_id
        });
    }
}

} // namespace md
