#include "main/MdEventConverter.hpp"

#include <cctype>
#include <ctime>
#include <iomanip>
#include <optional>
#include <simdjson.h>
#include <sstream>
#include <string>

namespace cmf
{

namespace
{

std::int64_t daysFromCivil(int year, unsigned month, unsigned day)
{
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

} // namespace

bool MdEventConverter::parseRaw(const std::string& rawLine, MarketDataEvent& event) const
{
    try
    {
        simdjson::ondemand::parser parser;
        simdjson::padded_string paddedLine(rawLine);
        simdjson::ondemand::document doc = parser.iterate(paddedLine);

        const auto tsRecvRaw = std::string(std::string_view(doc["ts_recv"].get_string()));
        const auto tsRecv = parseIso8601ToUnixNanos(tsRecvRaw);
        if (!tsRecv.has_value())
        {
            return false;
        }
        event.tsRecv = *tsRecv;

        simdjson::ondemand::object hd = doc["hd"].get_object();
        const auto tsEventRaw = std::string(std::string_view(hd["ts_event"].get_string()));
        const auto tsEvent = parseIso8601ToUnixNanos(tsEventRaw);
        if (!tsEvent.has_value())
        {
            return false;
        }
        event.hd.tsEvent = *tsEvent;
        event.hd.rtype = static_cast<std::uint16_t>(std::uint64_t(hd["rtype"].get_uint64()));
        event.hd.publisherId = static_cast<std::uint16_t>(std::uint64_t(hd["publisher_id"].get_uint64()));
        event.hd.instrumentId = static_cast<std::uint32_t>(std::uint64_t(hd["instrument_id"].get_uint64()));

        const auto actionString = std::string(std::string_view(doc["action"].get_string()));
        const auto sideString = std::string(std::string_view(doc["side"].get_string()));
        if (actionString.size() != 1 || sideString.size() != 1)
        {
            return false;
        }
        const auto action = actionFromChar(actionString[0]);
        const auto side = sideFromChar(sideString[0]);
        if (!action.has_value() || !side.has_value() || !isValidActionSide(*action, *side))
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
    catch (const simdjson::simdjson_error&)
    {
        return false;
    }
}

std::optional<MdAction> MdEventConverter::actionFromChar(char value)
{
    switch (value)
    {
    case 'A':
        return MdAction::Add;
    case 'M':
        return MdAction::Modify;
    case 'C':
        return MdAction::Cancel;
    case 'R':
        return MdAction::Clear;
    case 'T':
        return MdAction::Trade;
    case 'F':
        return MdAction::Fill;
    case 'N':
        return MdAction::None;
    default:
        return std::nullopt;
    }
}

std::optional<MdSide> MdEventConverter::sideFromChar(char value)
{
    switch (value)
    {
    case 'A':
        return MdSide::Ask;
    case 'B':
        return MdSide::Bid;
    case 'N':
        return MdSide::None;
    default:
        return std::nullopt;
    }
}

bool MdEventConverter::isValidActionSide(MdAction action, MdSide side)
{
    if (action == MdAction::Clear)
    {
        return side == MdSide::None;
    }
    return side == MdSide::Ask || side == MdSide::Bid || side == MdSide::None;
}

char MdEventConverter::toChar(MdAction value) { return static_cast<char>(value); }
char MdEventConverter::toChar(MdSide value) { return static_cast<char>(value); }

std::string MdEventConverter::formatUnixNanosToIso8601(std::int64_t unixNanos)
{
    const std::int64_t seconds = unixNanos / 1'000'000'000LL;
    const std::int64_t nanos = unixNanos % 1'000'000'000LL;

    std::time_t timeSeconds = static_cast<std::time_t>(seconds);
    std::tm tmUtc{};
#if defined(_WIN32)
    gmtime_s(&tmUtc, &timeSeconds);
#else
    gmtime_r(&timeSeconds, &tmUtc);
#endif

    std::ostringstream out;
    out << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%S") << "." << std::setw(9) << std::setfill('0')
        << nanos << "Z";
    return out.str();
}

std::optional<std::int64_t> MdEventConverter::parseIso8601ToUnixNanos(const std::string& value)
{
    // Expected format: YYYY-MM-DDTHH:MM:SS.NNNNNNNNNZ
    if (value.size() != 30 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value[19] != '.' || value[29] != 'Z')
    {
        return std::nullopt;
    }

    auto parsePart = [&](std::size_t start, std::size_t len) -> std::optional<int>
    {
        int out = 0;
        for (std::size_t i = 0; i < len; ++i)
        {
            const char c = value[start + i];
            if (!std::isdigit(static_cast<unsigned char>(c)))
            {
                return std::nullopt;
            }
            out = out * 10 + (c - '0');
        }
        return out;
    };

    const auto year = parsePart(0, 4);
    const auto month = parsePart(5, 2);
    const auto day = parsePart(8, 2);
    const auto hour = parsePart(11, 2);
    const auto minute = parsePart(14, 2);
    const auto second = parsePart(17, 2);
    const auto nanos = parsePart(20, 9);
    if (!year.has_value() || !month.has_value() || !day.has_value() || !hour.has_value() ||
        !minute.has_value() || !second.has_value() || !nanos.has_value())
    {
        return std::nullopt;
    }

    if (*month < 1 || *month > 12 || *day < 1 || *day > 31 || *hour > 23 || *minute > 59 ||
        *second > 60)
    {
        return std::nullopt;
    }

    const std::int64_t days = daysFromCivil(*year, static_cast<unsigned>(*month), static_cast<unsigned>(*day));
    const std::int64_t seconds = days * 86400 + *hour * 3600 + *minute * 60 + *second;
    return seconds * 1'000'000'000LL + *nanos;
}

std::optional<std::int64_t> MdEventConverter::parsePriceDecimalString(const std::string& raw)
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

} // namespace cmf
