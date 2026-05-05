#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace cmf
{

class MarketDataEvent
{
  public:
    struct Header
    {
        std::string tsEvent;
        std::uint16_t rtype{};
        std::uint16_t publisherId{};
        std::uint32_t instrumentId{};
    };

    std::string tsRecv;
    Header hd;
    char action{};
    char side{};
    std::optional<std::int64_t> price;
    std::uint32_t size{};
    std::uint32_t channelId{};
    std::string orderId;
    std::uint32_t flags{};
    std::int32_t tsInDelta{};
    std::uint64_t sequence{};
    std::string symbol;
};

} // namespace cmf
