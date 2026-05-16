#include "strategies/FixedQuoteStrategy.hpp"

#include <algorithm>

namespace md {
namespace {

std::uint64_t normalizedInterval(std::uint64_t interval) {
    return std::max<std::uint64_t>(interval, 1);
}

std::int64_t quantityAsSigned(std::uint64_t quantity) {
    return static_cast<std::int64_t>(quantity);
}

} // namespace

FixedQuoteStrategy::FixedQuoteStrategy(FixedQuoteStrategyConfig config)
    : config_(config) {}

std::string FixedQuoteStrategy::name() const {
    return "fixed_quote";
}

std::vector<StrategyAction> FixedQuoteStrategy::onMarketData(
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
    const auto mid_price = *market.mid_price;
    const auto offset = quoteOffset();

    if (canPlaceBuy(current_inventory)) {
        actions.push_back(PlaceOrderAction{
            .instrument_id = config_.instrument_id,
            .side = SimOrderSide::Buy,
            .price = mid_price - offset,
            .quantity = config_.order_size
        });
    }

    if (canPlaceSell(current_inventory)) {
        actions.push_back(PlaceOrderAction{
            .instrument_id = config_.instrument_id,
            .side = SimOrderSide::Sell,
            .price = mid_price + offset,
            .quantity = config_.order_size
        });
    }

    return actions;
}

bool FixedQuoteStrategy::shouldQuote(const MarketDataEvent& event, const MarketView& market) {
    if (event.instrument_id != config_.instrument_id
        || market.instrument_id != config_.instrument_id
        || !market.mid_price.has_value()
        || config_.order_size == 0) {
        return false;
    }

    ++eligible_event_count_;
    const auto interval = normalizedInterval(config_.quote_interval_events);
    return (eligible_event_count_ - 1) % interval == 0;
}

std::int64_t FixedQuoteStrategy::inventory(const Portfolio& portfolio) const {
    const auto* position = portfolio.findPosition(config_.instrument_id);
    return position == nullptr ? 0 : position->inventory;
}

std::int64_t FixedQuoteStrategy::quoteOffset() const {
    return config_.quote_offset_ticks * config_.tick_size;
}

bool FixedQuoteStrategy::canPlaceBuy(std::int64_t current_inventory) const {
    return current_inventory + quantityAsSigned(config_.order_size) <= config_.max_inventory;
}

bool FixedQuoteStrategy::canPlaceSell(std::int64_t current_inventory) const {
    return current_inventory - quantityAsSigned(config_.order_size) >= -config_.max_inventory;
}

void FixedQuoteStrategy::appendCancelActions(
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
