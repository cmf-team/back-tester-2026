#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace cmf
{

class MarketDataEvent
{
  public:
    enum class Action : char
    {
        Add = 'A',
        Modify = 'M',
        Cancel = 'C',
        Clear = 'R',
        Trade = 'T',
        Fill = 'F',
        None = 'N'
    };

    enum class Side : char
    {
        Ask = 'A',
        Bid = 'B',
        None = 'N'
    };

    struct Header
    {
        std::string tsEvent;
        std::uint16_t rtype{};
        std::uint16_t publisherId{};
        std::uint32_t instrumentId{};
    };

    static std::optional<Action> actionFromChar(char value)
    {
        switch (value)
        {
        case 'A':
            return Action::Add;
        case 'M':
            return Action::Modify;
        case 'C':
            return Action::Cancel;
        case 'R':
            return Action::Clear;
        case 'T':
            return Action::Trade;
        case 'F':
            return Action::Fill;
        case 'N':
            return Action::None;
        default:
            return std::nullopt;
        }
    }

    static std::optional<Side> sideFromChar(char value)
    {
        switch (value)
        {
        case 'A':
            return Side::Ask;
        case 'B':
            return Side::Bid;
        case 'N':
            return Side::None;
        default:
            return std::nullopt;
        }
    }

    static char toChar(Action value) { return static_cast<char>(value); }

    static char toChar(Side value) { return static_cast<char>(value); }

    static bool isValidActionSide(Action action, Side side)
    {
        if (action == Action::Clear)
        {
            return side == Side::None;
        }
        return side == Side::Ask || side == Side::Bid || side == Side::None;
    }

    std::string tsRecv;
    Header hd;
    Action action{Action::None};
    Side side{Side::None};
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
