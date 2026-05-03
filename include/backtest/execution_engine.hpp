#pragma once

#include "backtest/order.hpp"
#include "backtest/market_event.hpp"
#include <cstdint>

namespace backtest {

struct alignas(16) ExecutionReport {
    int64_t     order_id;       //  8   offset  0
    int64_t     avg_price;      //  8   offset  8
    int64_t     commission;     //  8   offset  16
    int64_t     timestamp_us;   //  8   offset  24
    int32_t     filled_qty;     //  4   offset  32
    OrderStatus status;         //  1   offset  36
    uint8_t     pad_[11];       //  11   offset  37 // NOLINT

    [[nodiscard]] static ExecutionReport rejected(int64_t order_id, int64_t timestamp_us) noexcept {
        return ExecutionReport{order_id, 0, 0, timestamp_us, 0, OrderStatus::Rejected, {}};
    }

    [[nodiscard]] static ExecutionReport filled(int64_t order_id, int32_t qty,
                                                 int64_t price, int64_t commission,
                                                 int64_t timestamp_us) noexcept {
        return ExecutionReport{order_id, price, commission, timestamp_us, qty, OrderStatus::Filled, {}};
    }
};

class ExecutionEngine {
public:

    explicit ExecutionEngine(int64_t commission_bps = 10) noexcept
        : commission_bps_(commission_bps) {}

    [[nodiscard]] ExecutionReport checkLimitOrder(const Order& order,
                                                   const MarketEvent& event) noexcept {

        if (!order.isActive() || order.type != OrderType::Limit) {
            return ExecutionReport::rejected(order.order_id, event.timestamp_us);
        }

        // price-cross критерий
        bool executed = false;

        if (order.side == OrderSide::Buy) {
            executed = (event.price_ticks <= order.price_ticks);
        } else {
            executed = (event.price_ticks >= order.price_ticks);
        }

        if (!executed) {
            return ExecutionReport::rejected(order.order_id, event.timestamp_us);
        }

        return executeOrder(order, event.price_ticks, event.timestamp_us);
    }


    [[nodiscard]] ExecutionReport executeMarketOrder(const Order& order,
                                                      const MarketEvent& event) noexcept {
        if (!order.isActive() || order.type != OrderType::Market) {
            return ExecutionReport::rejected(order.order_id, event.timestamp_us);
        }

        return executeOrder(order, event.price_ticks, event.timestamp_us);
    }


    [[nodiscard]] int64_t getCommissionBps() const noexcept { return commission_bps_; }
    void setCommissionBps(int64_t bps) noexcept { commission_bps_ = bps; }

private:

    [[nodiscard]] ExecutionReport executeOrder(const Order& order, int64_t exec_price,
                                                int64_t timestamp_us) noexcept {
        int64_t notional = exec_price * order.remainingQty();
        int64_t commission = (notional * commission_bps_) / 100'000;

        return ExecutionReport::filled(
            order.order_id,
            order.remainingQty(),
            exec_price,
            commission,
            timestamp_us
        );
    }

    int64_t commission_bps_;  // Комиссия в базисных пунктах (1 bp = 0.01%)
};

}