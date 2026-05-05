#pragma once

#include "common/MdEnums.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace cmf
{

struct MarketDataEvent
{
    struct Header
    {
        std::int64_t tsEvent{};
        std::uint16_t rtype{};
        std::uint16_t publisherId{};
        std::uint32_t instrumentId{};
    };

    std::int64_t tsRecv{};
    Header hd;
    MdAction action{MdAction::None};
    MdSide side{MdSide::None};
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
