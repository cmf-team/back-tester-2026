#pragma once


#include <cstddef>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>

#include "BasicTypes.hpp"
#include "MarketDataEvent.hpp"
#include "OrderState.hpp"


namespace cmf {

class LimitOrderBook {
public:
    void apply(const MarketDataEvent& event); // this method does change the object

    std::optional<std::pair<Price, Quantity>> bestBid() const; // const here not to change objects
    std::optional<std::pair<Price, Quantity>> bestAsk() const;

    Quantity volumeAtPrice(Side side, Price price) const;
    void printSnapshot(std::size_t depth = 5) const;

    std::uint64_t tradeCount() const { return trade_count_; }
    Quantity tradeVolume() const { return trade_volume_; }

private:
    void onAdd(const MarketDataEvent& event);
    void onCancel(const MarketDataEvent& event);
    void onModify(const MarketDataEvent& event);
    void onTrade(const MarketDataEvent& event);
    void onFill(const MarketDataEvent& event);
    void reduceOrder(OrderId orderId, Quantity executedQty);

    std::unordered_map<OrderId, OrderState> orders_;

    std::map<Price, Quantity, std::greater<Price>> bids_;
    std::map<Price, Quantity> asks_;

    std::uint64_t trade_count_ = 0;
    Quantity trade_volume_ = 0;
};

} // namespace cmf