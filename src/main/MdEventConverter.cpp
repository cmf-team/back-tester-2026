#include "main/MdEventConverter.hpp"

#include <optional>
#include <simdjson.h>
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
    return era * 146097LL + static_cast<int>(doe) - 719468;
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
        const auto tsRecv = isoTimestampToNanos(tsRecvRaw);
        if (!tsRecv.has_value())
        {
            return false;
        }
        event.tsRecv = *tsRecv;

        simdjson::ondemand::object hd = doc["hd"].get_object();
        const auto tsEventRaw = std::string(std::string_view(hd["ts_event"].get_string()));
        const auto tsEvent = isoTimestampToNanos(tsEventRaw);
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

        event.size = static_cast<Quantity>(std::uint64_t(doc["size"].get_uint64()));
        event.channelId = static_cast<std::uint32_t>(std::uint64_t(doc["channel_id"].get_uint64()));
        const auto orderIdStr = std::string(std::string_view(doc["order_id"].get_string()));
        try
        {
            std::size_t parsedChars = 0;
            const auto parsedOrderId = std::stoull(orderIdStr, &parsedChars);
            if (parsedChars != orderIdStr.size())
            {
                return false;
            }
            event.orderId = static_cast<OrderId>(parsedOrderId);
        }
        catch (const std::exception&)
        {
            return false;
        }
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

std::optional<Side> MdEventConverter::sideFromChar(char value)
{
    switch (value)
    {
    case 'A':
        return Side::Sell;
    case 'B':
        return Side::Buy;
    case 'N':
        return Side::None;
    default:
        return std::nullopt;
    }
}

bool MdEventConverter::isValidActionSide(MdAction action, Side side)
{
    if (action == MdAction::Clear)
    {
        return side == Side::None;
    }
    return side == Side::Sell || side == Side::Buy || side == Side::None;
}

char MdEventConverter::toChar(MdAction value) { return static_cast<char>(value); }
char MdEventConverter::toChar(Side value)
{
    switch (value)
    {
    case Side::Buy:
        return 'B';
    case Side::Sell:
        return 'A';
    case Side::None:
        return 'N';
    default:
        return '?';
    }
}

void MdEventConverter::civilFromDays(std::int64_t days, int& year, unsigned& month, unsigned& day)
{
    const std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    day = doy - (153 * mp + 2) / 5 + 1;
    month = mp < 10 ? mp + 3 : mp - 9;
    year = y + (month <= 2 ? 1 : 0);
}

void MdEventConverter::nanosToIsoTimestamp(std::int64_t unixNanos, char (&buf)[31])
{
    const std::int64_t seconds = unixNanos / 1'000'000'000LL;
    const std::int64_t nanos = unixNanos % 1'000'000'000LL;
    const std::int64_t days = seconds / 86400;
    const std::int64_t timeOfDay = seconds % 86400;

    int year;
    unsigned month;
    unsigned day;
    civilFromDays(days, year, month, day);

    const int hour = static_cast<int>(timeOfDay / 3600);
    const int minute = static_cast<int>((timeOfDay % 3600) / 60);
    const int second = static_cast<int>(timeOfDay % 60);

    std::snprintf(buf,
                  sizeof(buf),
                  "%04d-%02u-%02uT%02d:%02d:%02d.%09lldZ",
                  year,
                  month,
                  day,
                  hour,
                  minute,
                  second,
                  static_cast<long long>(nanos));
}

std::optional<std::int64_t> MdEventConverter::isoTimestampToNanos(const std::string& value)
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

std::optional<Price> MdEventConverter::parsePriceDecimalString(const std::string& raw)
{
    if (raw.empty())
    {
        return std::nullopt;
    }
    try
    {
        std::size_t parsedChars = 0;
        const double parsed = std::stod(raw, &parsedChars);
        if (parsedChars != raw.size())
        {
            return std::nullopt;
        }
        return parsed;
    }
    catch (const std::exception&)
    {
        return std::nullopt;
    }
}

} // namespace cmf
