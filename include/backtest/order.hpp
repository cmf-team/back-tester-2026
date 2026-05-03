#pragma once

#include <cstdint>

namespace backtest {

enum class OrderSide : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1
};

enum class OrderStatus : uint8_t {
    New = 0,
    PartiallyFilled = 1,
    Filled = 2,
    Cancelled = 3,
    Rejected = 4
};

struct alignas(16) Order {
    int64_t     order_id;       //  8   offset  0
    int64_t     timestamp_us;   //  8   offset  8
    int64_t     price_ticks;    //  8   offset  16
    int32_t     quantity;       //  4   offset  24
    int32_t     filled_qty;     //  4   offset  28
    OrderSide   side;           //  1   offset  32
    OrderType   type;           //  1   offset  33
    OrderStatus status;         //  1   offset  34
    uint8_t     pad_[13];        //  13   offset  35 // NOLINT

    [[nodiscard]] static Order limit_buy(int64_t price_ticks, int32_t quantity,
                                      int64_t order_id, int64_t timestamp_us) noexcept {
        return Order{order_id, timestamp_us, price_ticks, quantity, 0, OrderSide::Buy, OrderType::Limit, OrderStatus::New, {}};
    }

    [[nodiscard]] static Order limit_sell(int64_t price_ticks, int32_t quantity,
                                           int64_t order_id, int64_t timestamp_us) noexcept {
        return Order{order_id, timestamp_us, price_ticks, quantity, 0, OrderSide::Sell, OrderType::Limit, OrderStatus::New, {}};
    }

    [[nodiscard]] static Order market_buy(int32_t quantity,
                                           int64_t order_id, int64_t timestamp_us) noexcept {
        return Order{order_id, timestamp_us, 0, quantity, 0, OrderSide::Buy, OrderType::Market, OrderStatus::New, {}};
    }

    [[nodiscard]] static Order market_sell(int32_t quantity,
                                            int64_t order_id, int64_t timestamp_us) noexcept {
        return Order{order_id, timestamp_us, 0, quantity, 0, OrderSide::Sell, OrderType::Market, OrderStatus::New, {}};
    }

    [[nodiscard]] int32_t remainingQty() const noexcept {
        return quantity - filled_qty;
    }

    [[nodiscard]] bool isActive() const noexcept {
        return status == OrderStatus::New || status == OrderStatus::PartiallyFilled;
    }

    [[nodiscard]] bool isFilled() const noexcept {
        return status == OrderStatus::Filled;
    }
};

}