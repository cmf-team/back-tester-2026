#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct MarketDataEvent {
    std::string ts_recv;
    std::string ts_event;

    std::uint32_t rtype = 0;
    std::uint32_t publisher_id = 0;
    std::uint32_t instrument_id = 0;

    char action = '\0';
    char side = '\0';

    std::optional<double> price;
    std::uint32_t size = 0;

    std::uint32_t channel_id = 0;
    std::string order_id;

    std::uint32_t flags = 0;
    std::int32_t ts_in_delta = 0;
    std::uint64_t sequence = 0;

    std::string symbol;
};