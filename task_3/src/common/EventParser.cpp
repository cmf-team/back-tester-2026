#include "EventParser.hpp"
#include <fstream>
#include <simdjson.h>
#include <stdexcept>

namespace
{

bool map_action(std::string_view action, EventType& out)
{
    if (action == "A")
    {
        out = EventType::Add;
        return true;
    }
    if (action == "C")
    {
        out = EventType::Cancel;
        return true;
    }
    if (action == "M")
    {
        out = EventType::Modify;
        return true;
    }
    if (action == "T")
    {
        out = EventType::Trade;
        return true;
    }
    if (action == "F")
    {
        out = EventType::Fill;
        return true;
    }
    if (action == "R")
    {
        out = EventType::Reset;
        return true;
    }
    return false;
}

// Parse "2026-03-09T07:52:41.368148840Z" → nanoseconds since epoch
uint64_t parse_iso_ts(std::string_view s)
{
    if (s.size() < 20)
        return 0;
    auto g2 = [&](size_t p) -> uint64_t
    { return (s[p] - '0') * 10 + (s[p + 1] - '0'); };
    auto g4 = [&](size_t p) -> uint64_t
    {
        return (s[p] - '0') * 1000 + (s[p + 1] - '0') * 100 + (s[p + 2] - '0') * 10 + (s[p + 3] - '0');
    };
    uint64_t year = g4(0), month = g2(5), day = g2(8);
    uint64_t hour = g2(11), min = g2(14), sec = g2(17);
    uint64_t nanos = 0;
    int nd = 0;
    if (s.size() > 20 && s[19] == '.')
    {
        for (size_t i = 20; i < s.size() && s[i] >= '0' && s[i] <= '9' && nd < 9; ++i, ++nd)
            nanos = nanos * 10 + (s[i] - '0');
        while (nd < 9)
        {
            nanos *= 10;
            ++nd;
        }
    }
    static const uint16_t dim[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    auto leap = [](uint64_t y)
    { return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0); };
    uint64_t days = 0;
    for (uint64_t y = 1970; y < year; ++y)
        days += leap(y) ? 366 : 365;
    for (uint64_t m = 1; m < month; ++m)
    {
        days += dim[m];
        if (m == 2 && leap(year))
            ++days;
    }
    days += day - 1;
    return (days * 86400ULL + hour * 3600ULL + min * 60ULL + sec) * 1'000'000'000ULL + nanos;
}

// Parse "0.021200000" or null → int64 scaled 1e9
int64_t parse_price_str(std::string_view s)
{
    if (s.empty())
        return 0;
    bool neg = false;
    size_t i = 0;
    if (s[i] == '-')
    {
        neg = true;
        ++i;
    }
    int64_t intg = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9')
        intg = intg * 10 + (s[i++] - '0');
    int64_t frac = 0;
    int fd = 0;
    if (i < s.size() && s[i] == '.')
    {
        ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9' && fd < 9)
        {
            frac = frac * 10 + (s[i++] - '0');
            ++fd;
        }
        while (fd < 9)
        {
            frac *= 10;
            ++fd;
        }
    }
    int64_t r = intg * 1'000'000'000LL + frac;
    return neg ? -r : r;
}

// Parse order_id string "10996414798222631105" → uint64
uint64_t parse_u64_str(std::string_view s)
{
    uint64_t v = 0;
    for (char c : s)
        if (c >= '0' && c <= '9')
            v = v * 10 + (c - '0');
    return v;
}

} // namespace

bool parse_line(std::string_view line, MarketDataEvent& out)
{
    simdjson::ondemand::parser parser;
    auto doc = parser.iterate(line.data(), line.size(),
                              line.size() + simdjson::SIMDJSON_PADDING);
    if (doc.error())
        return false;

    // action
    std::string_view action;
    if (doc["action"].get(action))
        return false;
    EventType type{};
    if (!map_action(action, type))
        return false;

    // side
    Side side = Side::Bid;
    if (type != EventType::Reset)
    {
        std::string_view side_sv;
        if (doc["side"].get(side_sv))
            return false;
        if (side_sv == "B")
            side = Side::Bid;
        else if (side_sv == "A")
            side = Side::Ask;
        else
            return false;
    }

    // ts_recv — top-level ISO8601 string
    uint64_t ts_recv = 0;
    {
        std::string_view v;
        if (!doc["ts_recv"].get(v))
            ts_recv = parse_iso_ts(v);
    }

    // hd.ts_event + hd.instrument_id — nested object
    uint64_t ts_event = 0, instrument_id = 0;
    {
        simdjson::ondemand::object hd;
        if (!doc["hd"].get_object().get(hd))
        {
            std::string_view v;
            if (!hd["ts_event"].get(v))
                ts_event = parse_iso_ts(v);
            uint64_t iid = 0;
            if (!hd["instrument_id"].get(iid))
                instrument_id = iid;
        }
    }

    // price — string "0.021200000" or null
    int64_t price = 0;
    {
        std::string_view v;
        if (!doc["price"].get(v))
            price = parse_price_str(v);
    }

    // size
    int64_t qty = 0;
    {
        int64_t v = 0;
        if (!doc["size"].get(v))
            qty = v;
    }

    // order_id — string containing uint64
    uint64_t order_id = 0;
    {
        std::string_view v;
        if (!doc["order_id"].get(v))
            order_id = parse_u64_str(v);
    }

    // flags
    uint8_t flags = 0;
    {
        uint64_t v = 0;
        if (!doc["flags"].get(v))
            flags = static_cast<uint8_t>(v);
    }

    out.type = type;
    out.ts = ts_recv;
    out.ts_event = ts_event;
    out.order_id = order_id;
    out.instrument_id = instrument_id;
    out.side = side;
    out.price = price;
    out.qty = qty;
    out.flags = flags;
    return true;
}

std::vector<MarketDataEvent> parse_file(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Cannot open: " + path);

    std::vector<MarketDataEvent> events;
    events.reserve(1 << 20);
    std::string line;
    line.reserve(512);

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] != '{')
            continue;
        std::string padded = line;
        padded.resize(line.size() + simdjson::SIMDJSON_PADDING, '\0');
        MarketDataEvent ev;
        if (parse_line(std::string_view(padded.data(), line.size()), ev))
            events.push_back(ev);
    }
    return events;
}
