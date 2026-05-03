#pragma once

#include "backtest/order.hpp"
#include <cstdint>
#include <type_traits>

namespace backtest {

struct TradeRecord {
    int64_t     trade_id;       // 8 offset 0
    int64_t     order_id;       // 8 offset 8
    int64_t     timestamp_us;   // 8 offset 16
    int64_t     exec_price;     // 8 offset 24
    int64_t     commission;     // 8 offset 32
    int64_t     entry_price;    // 8 offset 40
    int64_t     pnl_ticks;      // 8 offset 48
    int64_t     expected_price; // 8 offset 56
    int32_t     quantity;       // 4 offset 64
    OrderSide   side;           // 1 offset 68
    uint8_t     pad_[3];        // 3 offset 69 //NOLINT

    constexpr TradeRecord() noexcept = default;

    constexpr TradeRecord(const int64_t tid, const int64_t oid, const int64_t ts, const OrderSide s,
                          const int32_t qty, const int64_t exec, const int64_t comm,
                          const int64_t entry, const int64_t pnl, const int64_t expected
    ) noexcept
        : trade_id(tid), order_id(oid), timestamp_us(ts), exec_price(exec), commission(comm),
          entry_price(entry), pnl_ticks(pnl), expected_price(expected), quantity(qty), side(s),
          pad_{}
    {
    }

    [[nodiscard]] constexpr double execPriceUsd() const noexcept {
        return static_cast<double>(exec_price) / 10'000'000.0;
    }

    [[nodiscard]] constexpr double pnlUsd() const noexcept {
        return static_cast<double>(pnl_ticks) / 10'000'000.0;
    }

    [[nodiscard]] constexpr double commissionUsd() const noexcept {
        return static_cast<double>(commission) / 10'000'000.0;
    }

    [[nodiscard]] constexpr int64_t slippageTicks() const noexcept {
        return exec_price - expected_price;
    }

    [[nodiscard]] double slippageBps() const noexcept {
        if (expected_price == 0) return 0.0;
        return static_cast<double>(slippageTicks()) * 10'000
               / static_cast<double>(expected_price);
    }
};

static_assert(sizeof(TradeRecord) == 72 || sizeof(TradeRecord) == 80,
              "TradeRecord size unexpected (check padding)");
static_assert(std::is_trivially_copyable_v<TradeRecord>,
              "TradeRecord must be trivially copyable");
static_assert(std::is_standard_layout_v<TradeRecord>,
              "TradeRecord must be standard layout");
static_assert(!std::is_floating_point_v<decltype(TradeRecord::pnl_ticks)>,
              "TradeRecord must not contain double fields");

}