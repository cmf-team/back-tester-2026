#include "main/FileReader.hpp"

#include <optional>
#include <simdjson.h>
#include <stdexcept>
#include <string>

namespace cmf
{

namespace
{

std::optional<std::int64_t> parsePriceDecimalString(const std::string &raw)
{
    if (raw.empty())
    {
        return std::nullopt;
    }

    bool negative = false;
    std::size_t pos = 0;
    if (raw[pos] == '-')
    {
        negative = true;
        ++pos;
    }
    if (pos >= raw.size())
    {
        return std::nullopt;
    }

    std::int64_t whole = 0;
    while (pos < raw.size() && std::isdigit(static_cast<unsigned char>(raw[pos])) != 0)
    {
        whole = whole * 10 + static_cast<std::int64_t>(raw[pos] - '0');
        ++pos;
    }

    std::int64_t fraction = 0;
    int digits = 0;
    if (pos < raw.size() && raw[pos] == '.')
    {
        ++pos;
        while (pos < raw.size() && std::isdigit(static_cast<unsigned char>(raw[pos])) != 0)
        {
            if (digits < 9)
            {
                fraction = fraction * 10 + static_cast<std::int64_t>(raw[pos] - '0');
                ++digits;
            }
            ++pos;
        }
    }

    if (pos != raw.size())
    {
        return std::nullopt;
    }
    while (digits < 9)
    {
        fraction *= 10;
        ++digits;
    }

    std::int64_t scaled = whole * 1'000'000'000LL + fraction;
    if (negative)
    {
        scaled = -scaled;
    }
    return scaled;
}

} // namespace

FileReader::FileReader(const std::string &inputFilePath)
    : inputFile_(inputFilePath), inputFilePath_(inputFilePath)
{
    if (!inputFile_.is_open())
    {
        throw std::runtime_error("Failed to open file: " + inputFilePath);
    }
}

bool FileReader::readNextEvent(MarketDataEvent &event)
{
    std::string line;
    while (std::getline(inputFile_, line))
    {
        ++lineNumber_;
        if (parseEventFromLine(line, event))
        {
            return true;
        }
    }

    if (!inputFile_.eof())
    {
        throw std::runtime_error("Error while reading file: " + inputFilePath_);
    }

    return false;
}

bool FileReader::parseEventFromLine(const std::string &line, MarketDataEvent &event) const
{
    try
    {
        simdjson::ondemand::parser parser;
        simdjson::padded_string paddedLine(line);
        simdjson::ondemand::document doc = parser.iterate(paddedLine);

        event.tsRecv = std::string(std::string_view(doc["ts_recv"].get_string()));

        simdjson::ondemand::object hd = doc["hd"].get_object();
        event.hd.tsEvent = std::string(std::string_view(hd["ts_event"].get_string()));
        event.hd.rtype = static_cast<std::uint16_t>(std::uint64_t(hd["rtype"].get_uint64()));
        event.hd.publisherId = static_cast<std::uint16_t>(std::uint64_t(hd["publisher_id"].get_uint64()));
        event.hd.instrumentId = static_cast<std::uint32_t>(std::uint64_t(hd["instrument_id"].get_uint64()));

        const auto actionString = std::string(std::string_view(doc["action"].get_string()));
        const auto sideString = std::string(std::string_view(doc["side"].get_string()));
        if (actionString.size() != 1 || sideString.size() != 1)
        {
            return false;
        }
        const auto action = MarketDataEvent::actionFromChar(actionString[0]);
        const auto side = MarketDataEvent::sideFromChar(sideString[0]);
        if (!action.has_value() || !side.has_value() || !MarketDataEvent::isValidActionSide(*action, *side))
        {
            return false;
        }
        event.action = *action;
        event.side = *side;

        auto priceField = doc["price"];
        if (priceField.type() == simdjson::ondemand::json_type::null)
        {
            event.price = std::nullopt;
        }
        else
        {
            const auto priceStr = std::string(std::string_view(priceField.get_string()));
            const auto scaledPrice = parsePriceDecimalString(priceStr);
            if (!scaledPrice.has_value())
            {
                return false;
            }
            event.price = *scaledPrice;
        }

        event.size = static_cast<std::uint32_t>(std::uint64_t(doc["size"].get_uint64()));
        event.channelId = static_cast<std::uint32_t>(std::uint64_t(doc["channel_id"].get_uint64()));
        event.orderId = std::string(std::string_view(doc["order_id"].get_string()));
        event.flags = static_cast<std::uint32_t>(std::uint64_t(doc["flags"].get_uint64()));
        event.tsInDelta = static_cast<std::int32_t>(std::int64_t(doc["ts_in_delta"].get_int64()));
        event.sequence = std::uint64_t(doc["sequence"].get_uint64());
        event.symbol = std::string(std::string_view(doc["symbol"].get_string()));
        return true;
    }
    catch (const simdjson::simdjson_error &)
    {
        return false;
    }
}

} // namespace cmf
