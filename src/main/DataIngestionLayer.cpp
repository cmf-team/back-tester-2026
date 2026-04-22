#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.hpp"   // nlohmann/json single-header library  . I downloaded it from : https://github.com/nlohmann/json/releases/download/v3.12.0/json.hpp
#include "DataIngestionLayer.hpp"

using json = nlohmann::json;

namespace
{
    std::string trim(const std::string& s)
    {
        const auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch); });
        const auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();

        if (begin >= end)
            return {};

        return std::string(begin, end);
    }

    std::string jsonValueToString(const json& value)
    {
        if (value.is_null())
            return {};

        if (value.is_string())
            return value.get<std::string>();

        if (value.is_number_integer())
            return std::to_string(value.get<long long>());

        if (value.is_number_unsigned())
            return std::to_string(value.get<unsigned long long>());

        if (value.is_number_float())
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(9) << value.get<long double>();
            return oss.str();
        }

        return value.dump();
    }

    char jsonValueToChar(const json& value, char defaultValue = 'N')
    {
        if (value.is_string())
        {
            const std::string s = value.get<std::string>();
            if (!s.empty())
                return s.front();
        }
        return defaultValue;
    }
}

class MarketDataEvent
{
public:
    std::string tsRecv;
    std::string tsEvent;
    int rtype = 0;
    int publisherId = 0;
    long long instrumentId = 0;

    char action = 'N';
    char side = 'N';
    std::optional<std::string> price;
    long long size = 0;
    long long channelId = 0;
    std::string orderId;
    int flags = 0;
    long long tsInDelta = 0;
    unsigned long long sequence = 0;
    std::string symbol;

    static MarketDataEvent fromJson(const json& j)
    {
        MarketDataEvent e;

        e.tsRecv = j.at("ts_recv").get<std::string>();

        const json& hd = j.at("hd");
        e.tsEvent = hd.at("ts_event").get<std::string>();
        e.rtype = hd.at("rtype").get<int>();
        e.publisherId = hd.at("publisher_id").get<int>();
        e.instrumentId = hd.at("instrument_id").get<long long>();

        e.action = jsonValueToChar(j.at("action"), 'N');
        e.side = jsonValueToChar(j.at("side"), 'N');

        if (j.contains("price") && !j.at("price").is_null())
            e.price = jsonValueToString(j.at("price"));

        if (j.contains("size") && !j.at("size").is_null())
            e.size = j.at("size").get<long long>();

        if (j.contains("channel_id") && !j.at("channel_id").is_null())
            e.channelId = j.at("channel_id").get<long long>();

        if (j.contains("order_id"))
            e.orderId = jsonValueToString(j.at("order_id"));

        if (j.contains("flags") && !j.at("flags").is_null())
            e.flags = j.at("flags").get<int>();

        if (j.contains("ts_in_delta") && !j.at("ts_in_delta").is_null())
            e.tsInDelta = j.at("ts_in_delta").get<long long>();

        if (j.contains("sequence") && !j.at("sequence").is_null())
            e.sequence = j.at("sequence").get<unsigned long long>();

        if (j.contains("symbol") && !j.at("symbol").is_null())
            e.symbol = j.at("symbol").get<std::string>();

        return e;
    }

    std::string priceOrNull() const
    {
        return price.has_value() ? *price : "null";
    }
};

std::ostream& operator<<(std::ostream& os, const MarketDataEvent& e)
{
    os << "ts_event=" << e.tsEvent
        << ", order_id=" << e.orderId
        << ", side=" << e.side
        << ", price=" << e.priceOrNull()
        << ", size=" << e.size
        << ", action=" << e.action
        << ", instrument_id=" << e.instrumentId
        << ", symbol=\"" << e.symbol << "\"";
    return os;
}

void processMarketDataEvent(const MarketDataEvent& event, std::size_t eventIndex)
{
    // Called for every event.
    // To avoid flooding the console, print only the first few during streaming.
    if (eventIndex < 10)
        std::cout << "[verify] " << event << '\n';
}

int RunDataIngestionFile(const std::string& filePath)
{
    std::ifstream input(filePath);
    if (!input.is_open())
    {
        std::cerr << "Cannot open file: " << filePath << '\n';
        return 2;
    }

    std::vector<MarketDataEvent> first10;
    first10.reserve(10);

    std::deque<MarketDataEvent> last10;

    std::string firstTsEvent;
    std::string lastTsEvent;
    std::string firstTsRecv;
    std::string lastTsRecv;

    std::size_t validMessages = 0;
    std::size_t invalidLines = 0;
    std::size_t lineNumber = 0;

    std::string line;
    while (std::getline(input, line))
    {
        ++lineNumber;
        line = trim(line);
        if (line.empty())
            continue;

        try
        {
            const json j = json::parse(line);
            MarketDataEvent event = MarketDataEvent::fromJson(j);

            if (validMessages == 0)
            {
                firstTsEvent = event.tsEvent;
                firstTsRecv = event.tsRecv;
            }

            lastTsEvent = event.tsEvent;
            lastTsRecv = event.tsRecv;

            if (first10.size() < 10)
                first10.push_back(event);

            if (last10.size() == 10)
                last10.pop_front();
            last10.push_back(event);

            processMarketDataEvent(event, validMessages);
            ++validMessages;
        }
        catch (const std::exception& ex)
        {
            ++invalidLines;
            std::cerr << "[warning] line " << lineNumber
                << " skipped: " << ex.what() << '\n';
        }
    }

    std::cout << "\n===== SUMMARY =====\n";
    std::cout << "Valid messages processed: " << validMessages << '\n';
    std::cout << "Invalid lines skipped:     " << invalidLines << '\n';

    if (validMessages > 0)
    {
        std::cout << "First ts_event:            " << firstTsEvent << '\n';
        std::cout << "Last ts_event:             " << lastTsEvent << '\n';
        std::cout << "First ts_recv:             " << firstTsRecv << '\n';
        std::cout << "Last ts_recv:              " << lastTsRecv << '\n';
    }

    std::cout << "\n===== FIRST 10 EVENTS =====\n";
    for (std::size_t i = 0; i < first10.size(); ++i)
        std::cout << (i + 1) << ". " << first10[i] << '\n';

    std::cout << "\n===== LAST 10 EVENTS =====\n";
    std::size_t idx = 1;
    for (const auto& event : last10)
        std::cout << idx++ << ". " << event << '\n';

    return 0;
}