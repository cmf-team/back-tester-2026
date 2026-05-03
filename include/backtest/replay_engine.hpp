#pragma once

#include "backtest/strategy.hpp"

#include <vector>
#include <chrono>
#include <thread>
#include <cstdint>

namespace backtest {

template<Strategy StrategyType>
class ReplayEngine {
public:

    ReplayEngine(const std::vector<MarketEvent>& events,
                 StrategyType& strategy,
                 double time_multiplier = 0.0)
        : events_(events)
        , strategy_(strategy)
        , time_multiplier_(time_multiplier) {}

    void run() {
        auto start_time = std::chrono::steady_clock::now();

        strategy_.on_init();

        const bool time_controlled = (time_multiplier_ > 0.0);
        int64_t first_timestamp = 0;

        if (time_controlled && !events_.empty()) {
            first_timestamp = events_.front().timestamp_us;
        }

        for (const auto& event : events_) {
            if (time_controlled) {
                waitForEventTime(event.timestamp_us, first_timestamp, start_time);
            }

            strategy_.on_event(event);
            ++processed_count_;
        }

        strategy_.on_finish();

        auto end_time = std::chrono::steady_clock::now();
        elapsed_us_ = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time).count();
    }

    [[nodiscard]] size_t processedCount() const noexcept {
        return processed_count_;
    }

    [[nodiscard]] int64_t elapsedUs() const noexcept {
        return elapsed_us_;
    }

    [[nodiscard]] size_t totalEvents() const noexcept {
        return events_.size();
    }

    [[nodiscard]] double timeMultiplier() const noexcept {
        return time_multiplier_;
    }

private:

    void waitForEventTime(int64_t event_ts_us,
                          int64_t first_ts_us,
                          std::chrono::steady_clock::time_point real_start_time) const noexcept {
        const int64_t event_offset_us = event_ts_us - first_ts_us;

        const int64_t scaled_offset_us = static_cast<int64_t>(
            static_cast<double>(event_offset_us) / time_multiplier_
        );

        auto target_time = real_start_time + std::chrono::microseconds(scaled_offset_us);

        while (std::chrono::steady_clock::now() < target_time) {
            std::this_thread::yield();
        }
    }

    const std::vector<MarketEvent>& events_;
    StrategyType& strategy_;
    double time_multiplier_;

    size_t processed_count_ = 0;
    int64_t elapsed_us_ = 0;
};

}