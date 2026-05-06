#pragma once

#include "common/BasicTypes.hpp"
#include "common/MdEnums.hpp"

#include <optional>
#include <string>

namespace cmf
{

struct MarketDataEvent
{
    struct Header
    {
        NanoTime tsEvent{};
        std::uint16_t rtype{};
        std::uint16_t publisherId{};
        std::uint32_t instrumentId{};
    };

    NanoTime tsRecv{};
    Header hd;
    MdAction action{MdAction::None};
    Side side{Side::None};
    std::optional<Price> price;
    Quantity size{};
    std::uint32_t channelId{};
    OrderId orderId{};
    std::uint32_t flags{};
    std::int32_t tsInDelta{};
    std::uint64_t sequence{};
    std::string symbol;
};

} // namespace cmf
