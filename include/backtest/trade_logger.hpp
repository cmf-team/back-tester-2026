#pragma once

#include "backtest/trade_record.hpp"
#include "backtest/ring_buffer.hpp"
#include <cstdint>

namespace backtest {


class TradeLogger {
public:

    static constexpr size_t DEFAULT_CAPACITY = 1'048'576;
    RingBuffer<TradeRecord, DEFAULT_CAPACITY> buffer_;

    void logTrade(const TradeRecord& trade) noexcept {
        buffer_.push(trade);
        total_commission_ += trade.commission;
        total_pnl_ += trade.pnl_ticks;
        total_volume_ += trade.quantity;
        ++total_trades_count_;
    }

    template<typename Func>
    void forEachTrade(Func&& func) const noexcept {
        buffer_.forEach(std::forward<Func>(func));
    }

    [[nodiscard]] size_t totalTrades() const noexcept {
        return buffer_.size();
    }

    [[nodiscard]] size_t totalTradesCount() const noexcept {
        return total_trades_count_;
    }

    [[nodiscard]] int64_t totalVolume() const noexcept {
        return total_volume_;
    }

    [[nodiscard]] int64_t totalCommission() const noexcept {
        return total_commission_;
    }

    [[nodiscard]] int64_t totalPnl() const noexcept {
        return total_pnl_;
    }

    [[nodiscard]] constexpr size_t capacity() const noexcept {
        return buffer_.capacity();
    }

    void clear() noexcept {
        buffer_.clear();
        total_commission_ = 0;
        total_pnl_ = 0;
        total_volume_ = 0;
        total_trades_count_ = 0;
    }

private:
    size_t total_trades_count_ = 0;
    int64_t total_commission_ = 0;
    int64_t total_pnl_ = 0;
    int64_t total_volume_ = 0;
};

}