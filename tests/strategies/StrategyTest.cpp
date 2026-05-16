#include "TestSupport.hpp"

#include "strategies/Strategy.hpp"

#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace md::test {
namespace {

class MockStrategy final : public Strategy {
public:
    explicit MockStrategy(std::vector<StrategyAction> actions)
        : actions_(std::move(actions)) {}

    std::string name() const override {
        return "mock";
    }

    std::vector<StrategyAction> onMarketData(
        const MarketDataEvent& event,
        const MarketView& market,
        const Portfolio& portfolio,
        const OrderManager& orders
    ) override {
        (void)event;
        (void)market;
        (void)portfolio;
        (void)orders;
        return actions_;
    }

private:
    std::vector<StrategyAction> actions_;
};

std::vector<StrategyAction> runStrategy(Strategy& strategy) {
    MarketDataEvent event;
    MarketView market;
    Portfolio portfolio;
    OrderManager orders;

    return strategy.onMarketData(event, market, portfolio, orders);
}

} // namespace

void testStrategyInterfaceCanBeMocked() {
    std::unique_ptr<Strategy> strategy = std::make_unique<MockStrategy>(std::vector<StrategyAction>{});

    require(strategy->name() == "mock", "strategy_interface_can_be_mocked: name via interface");
    require(runStrategy(*strategy).empty(), "strategy_interface_can_be_mocked: onMarketData via interface");
}

void testMockStrategyReturnsPlaceOrderAction() {
    MockStrategy strategy{std::vector<StrategyAction>{
        PlaceOrderAction{
            .instrument_id = 42,
            .side = SimOrderSide::Buy,
            .price = 100,
            .quantity = 10
        }
    }};

    const auto actions = runStrategy(strategy);

    require(actions.size() == 1, "mock_strategy_returns_place_order_action: one action");
    const auto* place = std::get_if<PlaceOrderAction>(&actions.front());
    require(place != nullptr, "mock_strategy_returns_place_order_action: place action variant");
    require(place->instrument_id == 42, "mock_strategy_returns_place_order_action: instrument id");
    require(place->side == SimOrderSide::Buy, "mock_strategy_returns_place_order_action: side");
    require(place->price == 100, "mock_strategy_returns_place_order_action: price");
    require(place->quantity == 10, "mock_strategy_returns_place_order_action: quantity");
}

void testMockStrategyReturnsCancelOrderAction() {
    MockStrategy strategy{std::vector<StrategyAction>{
        CancelOrderAction{
            .client_order_id = 17
        }
    }};

    const auto actions = runStrategy(strategy);

    require(actions.size() == 1, "mock_strategy_returns_cancel_order_action: one action");
    const auto* cancel = std::get_if<CancelOrderAction>(&actions.front());
    require(cancel != nullptr, "mock_strategy_returns_cancel_order_action: cancel action variant");
    require(cancel->client_order_id == 17, "mock_strategy_returns_cancel_order_action: client order id");
}

} // namespace md::test
