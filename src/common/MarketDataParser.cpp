#include "MarketDataParser.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using json = nlohmann::json;

// Parses an ISO 8601 UTC timestamp ("...Z") to nanoseconds since UNIX epoch.
// Returns UNDEF_TIMESTAMP on empty or malformed input.
static auto isoToNanos(const std::string& iso) -> std::uint64_t
{
    if (iso.empty())
        return MarketDataEvent::UNDEF_TIMESTAMP;

    std::tm tm{};
    std::istringstream ss(iso);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail())
        return MarketDataEvent::UNDEF_TIMESTAMP;

    // timegm treats tm as UTC; mktime would use local time.
    auto whole_s = static_cast<std::uint64_t>(timegm(&tm));

    auto dot = iso.find('.');
    auto z = iso.find('Z');
    std::uint64_t frac_ns = 0;
    if (dot != std::string::npos && z != std::string::npos)
    {
        auto frac = iso.substr(dot + 1, z - dot - 1);
        while (frac.size() < 9)
            frac += '0';
        frac_ns = std::stoull(frac.substr(0, 9));
    }

    return whole_s * 1'000'000'000ULL + frac_ns;
}

auto nanosToIso(std::uint64_t ns) -> std::string
{
    if (ns == MarketDataEvent::UNDEF_TIMESTAMP)
        return "";

    auto whole_s = static_cast<time_t>(ns / 1'000'000'000ULL);
    auto frac_ns = static_cast<unsigned>(ns % 1'000'000'000ULL);

    struct tm* tmp = gmtime(&whole_s);
    if (!tmp)
        return "";

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", tmp);
    std::string result{buf};
    result += '.';

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(9) << frac_ns;
    result += oss.str();
    result += 'Z';

    return result;
}

static auto priceFromString(const std::string& s) -> std::int64_t
{
    return static_cast<std::int64_t>(std::round(std::stod(s) * 1e9));
}

auto parseNDJSON(const std::string& line) -> std::optional<MarketDataEvent>
{
    try
    {
        auto j = json::parse(line);
        auto hd = j.at("hd");

        MarketDataEvent e;
        e.ts_recv = isoToNanos(j.value("ts_recv", ""));
        e.ts_event = isoToNanos(hd.value("ts_event", ""));
        e.rtype = hd.value("rtype", RType::Mbp0);
        e.publisher_id = hd.value("publisher_id", 0u);
        e.instrument_id = hd.value("instrument_id", 0u);
        e.action = static_cast<Action>(j.at("action").get<std::string>().front());
        e.side = static_cast<Side>(j.at("side").get<std::string>().front());
        e.price = priceFromString(j.at("price").get<std::string>());
        e.size = j.value("size", 0u);
        e.channel_id = j.value("channel_id", 0u);
        e.order_id = std::stoull(j.at("order_id").get<std::string>());
        e.flag = j.value("flags", Flag::None);
        e.ts_in_delta = j.value("ts_in_delta", 0);

        return e;
    }
    catch (...)
    {
        return std::nullopt;
    }
}
