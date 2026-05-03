#pragma once

#include <string>
#include <string_view>
#include <vector>
#include "market_event.hpp"

namespace backtest {

class CsvDataLoader {
public:
    explicit CsvDataLoader(const std::string& filepath);

    [[nodiscard]] std::vector<MarketEvent>& load();

    [[nodiscard]] size_t eventCount() const noexcept;
    [[nodiscard]] int64_t startTime() const noexcept;
    [[nodiscard]] int64_t endTime() const noexcept;

private:
    static constexpr double price_tick_multiplier = 10'000'000.0;
    static bool parseLine(std::string_view line, MarketEvent& out);

    static Side parseSide(std::string_view token) noexcept;
    static int64_t parsePriceFixed(std::string_view svw) noexcept;

    std::string filepath_;
    std::vector<MarketEvent> events_;
    bool loaded_ = false;
};

}