#include "LimitOrderBook.hpp"
#include "PriceUtils.hpp"


#include <iostream>
#include <stdexcept>

namespace cmf {

void LimitOrderBook::apply(const MarketDataEvent& event) {
    if (event.action == 'A') {
        onAdd(event);
    } else if (event.action == 'C') {
        onCancel(event);
    } else if (event.action == 'M') {
        onModify(event);
    } else if (event.action == 'T') {
        onTrade(event);
    } else if (event.action == 'F') {
        onFill(event);
    }
}

void LimitOrderBook::onAdd(const MarketDataEvent& event) {
    if (event.order_id == 0) {
        throw std::runtime_error("Add event has zero order_id");
    }

    OrderState state;
    state.orderId = event.order_id;
    state.instrumentId = event.instrument_id;
    state.price = event.price;  // уже scaled integer Price
    state.qty = static_cast<Quantity>(event.size);

    if (event.side == Side::Buy) {
        state.side = Side::Buy;
    } else if (event.side == Side::Sell) {
        state.side = Side::Sell;
    } else {
        throw std::runtime_error("Add event has unknown side");
    }

    orders_[state.orderId] = state;

    if (state.side == Side::Buy) {
        bids_[state.price] += state.qty;
    } else {
        asks_[state.price] += state.qty;
    }
}


void LimitOrderBook::onCancel(const MarketDataEvent& event) {
    if (event.order_id == 0) {
        throw std::runtime_error("Cancel event has zero order_id");
    }

    auto orderIt = orders_.find(event.order_id);
    if (orderIt == orders_.end()) {
        return;
    }

    OrderState& state = orderIt->second;

    Quantity cancelQty = static_cast<Quantity>(event.size);
    if (cancelQty <= 0) {
        return;
    }
    if (cancelQty > state.qty) {
        cancelQty = state.qty;
    }

    if (state.side == Side::Buy) {
        auto levelIt = bids_.find(state.price);
        if (levelIt != bids_.end()) {
            levelIt->second -= cancelQty;
            if (levelIt->second <= 0) {
                bids_.erase(levelIt);
            }
        }
    } else if (state.side == Side::Sell) {
        auto levelIt = asks_.find(state.price);
        if (levelIt != asks_.end()) {
            levelIt->second -= cancelQty;
            if (levelIt->second <= 0) {
                asks_.erase(levelIt);
            }
        }
    }

    state.qty -= cancelQty;
    if (state.qty <= 0) {
        orders_.erase(orderIt);
    }
}

void LimitOrderBook::onModify(const MarketDataEvent& event) {
    if (event.order_id == 0) {
        throw std::runtime_error("Modify event has zero order_id");
    }

    auto orderIt = orders_.find(event.order_id);
    if (orderIt == orders_.end()) {
        return;
    }

    OrderState& state = orderIt->second;

    if (state.side == Side::Buy) {
        auto levelIt = bids_.find(state.price);
        if (levelIt != bids_.end()) {
            levelIt->second -= state.qty;
            if (levelIt->second <= 0) {
                bids_.erase(levelIt);
            }
        }
    } else if (state.side == Side::Sell) {
        auto levelIt = asks_.find(state.price);
        if (levelIt != asks_.end()) {
            levelIt->second -= state.qty;
            if (levelIt->second <= 0) {
                asks_.erase(levelIt);
            }
        }
    }

    if (event.price > 0) {
        state.price = event.price;
    }

    if (event.size > 0) {
        state.qty = static_cast<Quantity>(event.size);
    }

    if (event.side == Side::Buy || event.side == Side::Sell) {
        state.side = event.side;
    }

    if (state.side == Side::Buy) {
        bids_[state.price] += state.qty;
    } else if (state.side == Side::Sell) {
        asks_[state.price] += state.qty;
    }
}

void LimitOrderBook::reduceOrder(OrderId orderId, Quantity executedQty) {
    if (orderId == 0 || executedQty <= 0) {
        return;
    }

    auto orderIt = orders_.find(orderId);
    if (orderIt == orders_.end()) {
        return;
    }

    OrderState& state = orderIt->second;

    Quantity reduction = std::min(state.qty, executedQty);

    if (state.side == Side::Buy) {
        auto levelIt = bids_.find(state.price);
        if (levelIt != bids_.end()) {
            levelIt->second -= reduction;
            if (levelIt->second <= 0) {
                bids_.erase(levelIt);
            }
        }
    } else if (state.side == Side::Sell) {
        auto levelIt = asks_.find(state.price);
        if (levelIt != asks_.end()) {
            levelIt->second -= reduction;
            if (levelIt->second <= 0) {
                asks_.erase(levelIt);
            }
        }
    }

    state.qty -= reduction;

    if (state.qty <= 0) {
        orders_.erase(orderIt);
    }
}

void LimitOrderBook::onTrade(const MarketDataEvent& event) {
    ++trade_count_;
    trade_volume_ += static_cast<Quantity>(event.size);
}

void LimitOrderBook::onFill(const MarketDataEvent& event) {
    if (event.order_id == 0) {
        return;
    }

    reduceOrder(event.order_id, static_cast<Quantity>(event.size));
}

std::optional<std::pair<Price, Quantity>> LimitOrderBook::bestBid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }

    return *bids_.begin();
}

std::optional<std::pair<Price, Quantity>> LimitOrderBook::bestAsk() const {
    if (asks_.empty()) {
        return std::nullopt;
    }

    return *asks_.begin();
}

Quantity LimitOrderBook::volumeAtPrice(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return (it != bids_.end()) ? it->second : 0;
    }

    if (side == Side::Sell) {
        auto it = asks_.find(price);
        return (it != asks_.end()) ? it->second : 0;
    }

    return 0;
}

void LimitOrderBook::printSnapshot(std::size_t depth) const {
    std::cout << "----- LOB Snapshot -----\n";

    std::cout << "ASKS:\n";
    std::size_t askCount = 0;
    for (auto it = asks_.begin(); it != asks_.end() && askCount < depth; ++it, ++askCount) {
        std::cout << "price=" << formatScaledPrice(it->first)
                  << " qty=" << it->second << "\n";
    }

    std::cout << "BIDS:\n";
    std::size_t bidCount = 0;
    for (auto it = bids_.begin(); it != bids_.end() && bidCount < depth; ++it, ++bidCount) {
        std::cout << "price=" << formatScaledPrice(it->first)
                  << " qty=" << it->second << "\n";
    }
}

} // namespace cmf