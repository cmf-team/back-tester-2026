#include "SimpleDataParser.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "common/BasicTypes.hpp"

namespace cmf {

static std::string_view findValue(std::string_view line, std::string_view key) {
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
                                            std::string_view key) {
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
                                                std::string_view key) {
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

static std::string_view getObject(std::string_view line, std::string_view key) {
    auto rest = findValue(line, key);
    if (rest.empty() || rest.front() != '{')
        return {};
    std::size_t depth = 0;
    std::size_t len = 0;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        if (rest[i] == '{')
            ++depth;
        else if (rest[i] == '}') {
            --depth;
            if (depth == 0) {
                len = i + 1;
                break;
            }
        }
    }
    return rest.substr(0, len);
}

static bool isNull(std::string_view line, std::string_view key) {
    auto rest = findValue(line, key);
    return rest.size() >= 4 && rest.substr(0, 4) == "null";
}

static NanoTime parseIso8601Nanos(const std::string& s) {
    static constexpr std::int64_t POW10[10] = {
        1'000'000'000LL, 100'000'000LL, 10'000'000LL, 1'000'000LL, 100'000LL,
        10'000LL,        1'000LL,       100LL,        10LL,        1LL};

    std::tm tm{};
    const char* p = strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    if (!p)
        throw std::runtime_error("bad timestamp: " + s);

    time_t sec = timegm(&tm);
    std::int64_t nanos = 0;

    if (*p == '.') {
        ++p;
        int digits = 0;
        while (*p >= '0' && *p <= '9' && digits < 9) {
            nanos = nanos * 10 + (*p++ - '0');
            ++digits;
        }
        nanos *= POW10[digits];
    }

    return static_cast<NanoTime>(sec) * 1'000'000'000LL + nanos;
}

static MarketDataEvent parseLine(const std::string& line) {
    MarketDataEvent e;

    std::string_view sv(line);

    auto hd = getObject(sv, "hd");

    auto tsRecv = getString(sv, "ts_recv");
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
        e.publisher_id = static_cast<std::uint32_t>(std::stoul(*pubId));

    auto instId = getNumericRaw(hd, "instrument_id");
    if (instId)
        e.instrument_id = static_cast<std::uint32_t>(std::stoul(*instId));

    auto action = getString(sv, "action");
    if (action && !action->empty())
        e.action = (*action)[0];

    auto sideStr = getString(sv, "side");
    if (sideStr && !sideStr->empty()) {
        char c = (*sideStr)[0];
        if (c == 'B')
            e.side = Side::Buy;
        else if (c == 'A' || c == 'S')
            e.side = Side::Sell;
        else
            e.side = Side::None;
    }

    if (!isNull(sv, "price")) {
        auto priceStr = getString(sv, "price");
        if (priceStr) {
            // double → scaled integer by PriceScale
            double p = std::stod(*priceStr);
            e.price = static_cast<Price>(p * static_cast<double>(PriceScale));
        }
    }

    auto size = getNumericRaw(sv, "size");
    if (size)
        e.size = static_cast<std::uint32_t>(std::stoul(*size));

    auto chanId = getNumericRaw(sv, "channel_id");
    if (chanId)
        e.channel_id = static_cast<std::uint16_t>(std::stoul(*chanId));

    auto orderId = getString(sv, "order_id");
    if (orderId)
        e.order_id = std::stoull(*orderId);

    auto flags = getNumericRaw(sv, "flags");
    if (flags)
        e.flags = static_cast<Flags>(std::stoul(*flags));

    auto delta = getNumericRaw(sv, "ts_in_delta");
    if (delta)
        e.ts_in_delta = static_cast<std::int32_t>(std::stol(*delta));

    auto seq = getNumericRaw(sv, "sequence");
    if (seq)
        e.sequence = static_cast<std::uint32_t>(std::stoul(*seq));

    return e;
}

void SimpleDataParser::parse_inner(
    const std::function<void(const MarketDataEvent&)>& f) const {
    std::ifstream fs(path_);
    if (!fs.is_open())
        throw std::runtime_error("SimpleDataParser: cannot open " + path_.string());

    std::string line;
    while (std::getline(fs, line)) {
        if (line.empty())
            continue;
        try {
            f(parseLine(line));
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "SimpleDataParser: skipping line: %s\n", ex.what());
        }
    }
}

} // namespace cmf