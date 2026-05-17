#include "JsonSimpleDataParser.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cmf
{

static std::string_view findValue(std::string_view line, std::string_view key)
{
    std::string needle;
    needle.reserve(key.size() + 3);
    needle += '"';
    needle += key;
    needle += '"';
    needle += ':';

    auto pos = line.find(needle);
    if (pos == std::string_view::npos)
        return {};

    std::string_view rest = line.substr(pos + needle.size());
    while (!rest.empty() && rest.front() == ' ')
        rest.remove_prefix(1);

    return rest;
}

static std::optional<std::string> getString(std::string_view line,
                                            std::string_view key)
{
    auto rest = findValue(line, key);
    if (rest.empty() || rest.front() != '"')
        return std::nullopt;
    rest.remove_prefix(1);
    auto end = rest.find('"');
    if (end == std::string_view::npos)
        return std::nullopt;
    return std::string(rest.substr(0, end));
}

static std::optional<std::string> getNumericRaw(std::string_view line,
                                                std::string_view key)
{
    auto rest = findValue(line, key);
    if (rest.empty())
        return std::nullopt;
    if (rest.front() != '-' && (rest.front() < '0' || rest.front() > '9'))
        return std::nullopt;
    std::size_t len = 0;
    while (len < rest.size() &&
           (rest[len] == '-' || (rest[len] >= '0' && rest[len] <= '9')))
        ++len;
    return std::string(rest.substr(0, len));
}

static std::string_view getObject(std::string_view line, std::string_view key)
{
    auto rest = findValue(line, key);
    if (rest.empty() || rest.front() != '{')
        return {};
    std::size_t depth = 0;
    std::size_t len = 0;
    for (std::size_t i = 0; i < rest.size(); ++i)
    {
        if (rest[i] == '{')
            ++depth;
        else if (rest[i] == '}')
        {
            --depth;
            if (depth == 0)
            {
                len = i + 1;
                break;
            }
        }
    }
    return rest.substr(0, len);
}

static bool isNull(std::string_view line, std::string_view key)
{
    auto rest = findValue(line, key);
    return rest.size() >= 4 && rest.substr(0, 4) == "null";
}

static NanoTime parseIso8601Nanos(const std::string& s)
{
    // POW10[n] = 10^(9-n): scales n fractional digits to nanoseconds
    static constexpr int64_t POW10[10] = {
        1'000'000'000LL, 100'000'000LL, 10'000'000LL, 1'000'000LL, 100'000LL,
        10'000LL, 1'000LL, 100LL, 10LL, 1LL};
    struct tm tm{};
    const char* p = strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    if (!p)
        throw std::runtime_error("bad timestamp: " + s);
    time_t sec = timegm(&tm);
    int64_t nanos = 0;
    if (*p == '.')
    {
        ++p;
        int digits = 0;
        while (*p >= '0' && *p <= '9' && digits < 9)
        {
            nanos = nanos * 10 + (*p++ - '0');
            ++digits;
        }
        nanos *= POW10[digits];
    }
    return static_cast<NanoTime>(sec) * 1'000'000'000LL + nanos;
}

static MarketDataEvent parseLine(const std::string& line)
{
    MarketDataEvent e;

    auto hd = getObject(line, "hd");

    auto tsRecv = getString(line, "ts_recv");
    if (!tsRecv)
        throw std::runtime_error("missing ts_recv");
    e.ts_recv = parseIso8601Nanos(*tsRecv);

    auto tsEvent = getString(hd, "ts_event");
    if (tsEvent)
        e.ts_event = parseIso8601Nanos(*tsEvent);

    auto rtype = getNumericRaw(hd, "rtype");
    if (rtype)
        e.rtype = static_cast<RType>(std::stoul(*rtype));

    auto pubId = getNumericRaw(hd, "publisher_id");
    if (pubId)
        e.publisher_id = static_cast<uint32_t>(std::stoul(*pubId));

    auto instId = getNumericRaw(hd, "instrument_id");
    if (instId)
        e.instrument_id = static_cast<uint32_t>(std::stoul(*instId));

    auto action = getString(line, "action");
    if (action && !action->empty())
        e.action = static_cast<Action>((*action)[0]);

    auto side = getString(line, "side");
    if (side && !side->empty())
    {
        const char side_char = (*side)[0];
        switch (side_char)
        {
        case 'B':
            e.side = Side::Buy;
            break;
        case 'S':
        case 'A':
            e.side = Side::Sell;
            break;
        default:
            e.side = Side::None;
            break;
        }
    }

    if (!isNull(line, "price"))
    {
        auto price = getString(line, "price");
        if (price)
            e.price = static_cast<int64_t>(std::stod(*price) * 1e9);
    }

    auto size = getNumericRaw(line, "size");
    if (size)
        e.size = static_cast<uint32_t>(std::stoul(*size));

    auto chanId = getNumericRaw(line, "channel_id");
    if (chanId)
        e.channel_id = static_cast<uint16_t>(std::stoul(*chanId));

    auto orderId = getString(line, "order_id");
    if (orderId)
        e.order_id = std::stoull(*orderId);

    auto flags = getNumericRaw(line, "flags");
    if (flags)
        e.flags = static_cast<Flags>(std::stoul(*flags));

    auto delta = getNumericRaw(line, "ts_in_delta");
    if (delta)
        e.ts_in_delta = static_cast<int32_t>(std::stol(*delta));

    auto seq = getNumericRaw(line, "sequence");
    if (seq)
        e.sequence = static_cast<uint32_t>(std::stoul(*seq));

    return e;
}

void JsonSimpleDataParser::parse_inner(
    const std::function<void(const MarketDataEvent&)>& f) const
{
    if (!path_.string().ends_with(".mbo.json"))
        throw std::runtime_error(
            "SimpleDataParser: unsupported format, expected .mbo.json file, got " +
            path_.string());

    std::ifstream fs(path_);
    if (!fs.is_open())
        throw std::runtime_error("SimpleDataParser: cannot open " + path_.string());

    std::string line;
    while (std::getline(fs, line))
    {
        if (line.empty())
            continue;
        try
        {
            f(parseLine(line));
        }
        catch (const std::exception& ex)
        {
            std::fprintf(stderr, "SimpleDataParser: skipping line: %s\n", ex.what());
        }
    }
}

} // namespace cmf
