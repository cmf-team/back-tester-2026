#include "main/FileReader.hpp"

#include <cctype>
#include <cstdint>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace cmf
{

namespace
{

std::size_t skipWhitespace(const std::string &s, std::size_t pos)
{
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])) != 0)
    {
        ++pos;
    }
    return pos;
}

std::optional<std::size_t> findField(const std::string &line, const std::string &key, std::size_t startPos = 0)
{
    const std::string pattern = "\"" + key + "\"";
    std::size_t keyPos = line.find(pattern, startPos);
    if (keyPos == std::string::npos)
    {
        return std::nullopt;
    }
    std::size_t colonPos = line.find(':', keyPos + pattern.size());
    if (colonPos == std::string::npos)
    {
        return std::nullopt;
    }
    return skipWhitespace(line, colonPos + 1);
}

std::optional<std::string> parseStringField(const std::string &line,
                                            const std::string &key,
                                            std::size_t startPos = 0)
{
    const auto valuePos = findField(line, key, startPos);
    if (!valuePos.has_value() || *valuePos >= line.size() || line[*valuePos] != '"')
    {
        return std::nullopt;
    }

    const std::size_t begin = *valuePos + 1;
    const std::size_t end = line.find('"', begin);
    if (end == std::string::npos)
    {
        return std::nullopt;
    }

    return line.substr(begin, end - begin);
}

template <typename T> std::optional<T> parseIntegerField(const std::string &line,
                                                         const std::string &key,
                                                         std::size_t startPos = 0)
{
    const auto valuePos = findField(line, key, startPos);
    if (!valuePos.has_value() || *valuePos >= line.size())
    {
        return std::nullopt;
    }

    std::size_t endPos = *valuePos;
    if (line[endPos] == '-')
    {
        ++endPos;
    }
    while (endPos < line.size() && std::isdigit(static_cast<unsigned char>(line[endPos])) != 0)
    {
        ++endPos;
    }
    if (endPos == *valuePos || (endPos == *valuePos + 1 && line[*valuePos] == '-'))
    {
        return std::nullopt;
    }

    try
    {
        const std::string number = line.substr(*valuePos, endPos - *valuePos);
        if constexpr (std::is_signed_v<T>)
        {
            const auto parsed = std::stoll(number);
            if (parsed < static_cast<long long>(std::numeric_limits<T>::min()) ||
                parsed > static_cast<long long>(std::numeric_limits<T>::max()))
            {
                return std::nullopt;
            }
            return static_cast<T>(parsed);
        }
        else
        {
            if (number[0] == '-')
            {
                return std::nullopt;
            }
            const auto parsed = std::stoull(number);
            if (parsed > static_cast<unsigned long long>(std::numeric_limits<T>::max()))
            {
                return std::nullopt;
            }
            return static_cast<T>(parsed);
        }
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

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

std::optional<std::optional<std::int64_t>> parsePriceField(const std::string &line)
{
    const auto valuePos = findField(line, "price");
    if (!valuePos.has_value() || *valuePos >= line.size())
    {
        return std::nullopt;
    }

    if (line.compare(*valuePos, 4, "null") == 0)
    {
        return std::optional<std::int64_t>{};
    }

    if (line[*valuePos] == '"')
    {
        const std::size_t begin = *valuePos + 1;
        const std::size_t end = line.find('"', begin);
        if (end == std::string::npos)
        {
            return std::nullopt;
        }
        auto scaled = parsePriceDecimalString(line.substr(begin, end - begin));
        if (!scaled.has_value())
        {
            return std::nullopt;
        }
        return std::optional<std::int64_t>{*scaled};
    }

    return std::nullopt;
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
    const auto tsRecv = parseStringField(line, "ts_recv");
    const auto action = parseStringField(line, "action");
    const auto side = parseStringField(line, "side");
    const auto size = parseIntegerField<std::uint32_t>(line, "size");
    const auto channelId = parseIntegerField<std::uint32_t>(line, "channel_id");
    const auto orderId = parseStringField(line, "order_id");
    const auto flags = parseIntegerField<std::uint32_t>(line, "flags");
    const auto tsInDelta = parseIntegerField<std::int32_t>(line, "ts_in_delta");
    const auto sequence = parseIntegerField<std::uint64_t>(line, "sequence");
    const auto symbol = parseStringField(line, "symbol");
    const auto price = parsePriceField(line);

    const auto hdPos = findField(line, "hd");
    if (!hdPos.has_value() || *hdPos >= line.size() || line[*hdPos] != '{')
    {
        return false;
    }
    const std::size_t hdEnd = line.find('}', *hdPos);
    if (hdEnd == std::string::npos)
    {
        return false;
    }
    const auto tsEvent = parseStringField(line, "ts_event", *hdPos);
    const auto rtype = parseIntegerField<std::uint16_t>(line, "rtype", *hdPos);
    const auto publisherId = parseIntegerField<std::uint16_t>(line, "publisher_id", *hdPos);
    const auto instrumentId = parseIntegerField<std::uint32_t>(line, "instrument_id", *hdPos);

    if (!tsRecv.has_value() || !action.has_value() || action->size() != 1 || !side.has_value() ||
        side->size() != 1 || !size.has_value() || !channelId.has_value() || !orderId.has_value() ||
        !flags.has_value() || !tsInDelta.has_value() || !sequence.has_value() || !symbol.has_value() ||
        !price.has_value() || !tsEvent.has_value() || !rtype.has_value() || !publisherId.has_value() ||
        !instrumentId.has_value())
    {
        return false;
    }

    event.tsRecv = *tsRecv;
    event.hd.tsEvent = *tsEvent;
    event.hd.rtype = *rtype;
    event.hd.publisherId = *publisherId;
    event.hd.instrumentId = *instrumentId;
    event.action = (*action)[0];
    event.side = (*side)[0];
    event.price = *price;
    event.size = *size;
    event.channelId = *channelId;
    event.orderId = *orderId;
    event.flags = *flags;
    event.tsInDelta = *tsInDelta;
    event.sequence = *sequence;
    event.symbol = *symbol;
    return true;
}

} // namespace cmf
