#pragma once

#include "common/MarketDataEvent.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <istream>
#include <ostream>
#include <vector>

struct ParseSummary
{
    std::size_t total_events{0};
    std::uint64_t processing_time_ns{0};
};

void PresentToUser(std::ostream& out, std::istream& file);

void ProcessMarketDataEvent(const MarketDataEvent& event);
