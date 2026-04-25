#include "LimitOrderBook.hpp"
#include "common/MarketDataEvent.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

using namespace cmf;
using json = nlohmann::json;

int64_t getSafeInt64(const json& j, const std::string& k)
{
    if (!j.contains(k) || j[k].is_null())
        return 0LL;
    return j[k].is_string() ? std::stoll(j[k].get<std::string>()) : j[k].get<int64_t>();
}
uint64_t getSafeUint64(const json& j, const std::string& k)
{
    if (!j.contains(k) || j[k].is_null())
        return 0ULL;
    return j[k].is_string() ? std::stoull(j[k].get<std::string>()) : j[k].get<uint64_t>();
}
uint32_t getSafeUint32(const json& j, const std::string& k)
{
    if (!j.contains(k) || j[k].is_null())
        return 0;
    return j[k].is_string() ? static_cast<uint32_t>(std::stoul(j[k].get<std::string>())) : j[k].get<uint32_t>();
}
double getSafePrice(const json& j, const std::string& k)
{
    if (!j.contains(k) || j[k].is_null())
        return 0.0;
    if (j[k].is_string())
        return std::stod(j[k].get<std::string>());
    if (j[k].is_number_integer())
    {
        int64_t r = j[k].get<int64_t>();
        return (r == 9223372036854775807LL) ? 0.0 : static_cast<double>(r) / 1e9;
    }
    return j[k].get<double>();
}
double getSafeDouble(const json& j, const std::string& k)
{
    if (!j.contains(k) || j[k].is_null())
        return 0.0;
    return j[k].is_string() ? std::stod(j[k].get<std::string>()) : j[k].get<double>();
}
std::string getSafeString(const json& j, const std::string& k)
{
    if (!j.contains(k) || j[k].is_null() || !j[k].is_string())
        return "N";
    return j[k].get<std::string>();
}

Side mapDatabentoSide(const std::string& s)
{
    if (s.empty())
        return Side::None;
    return (s[0] == 'A') ? Side::Sell : ((s[0] == 'B') ? Side::Buy : Side::None);
}
Action mapAction(const std::string& a)
{
    return a.empty() ? Action::None : static_cast<Action>(a[0]);
}

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path_to_ndjson_file>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open())
        return 1;

    std::unordered_map<uint32_t, LimitOrderBook> lobs;

    std::string line;
    size_t processedCount = 0;
    uint32_t sample_instr_id = 0;

    std::cout << "Starting Sequential LOB Reconstruction...\n";
    auto start_time = std::chrono::high_resolution_clock::now();

    try
    {
        while (std::getline(file, line))
        {
            if (line.empty())
                continue;

            json j = json::parse(line);
            MarketDataEvent event;

            event.timestamp = getSafeInt64(j, "ts_recv");
            if (event.timestamp == 0)
                event.timestamp = getSafeInt64(j, "ts_event");

            event.instrument_id = getSafeUint32(j, "instrument_id");
            event.order_id = getSafeUint64(j, "order_id");
            event.price = getSafePrice(j, "price");
            event.size = getSafeDouble(j, "size");
            event.side = mapDatabentoSide(getSafeString(j, "side"));
            event.action = mapAction(getSafeString(j, "action"));

            if (processedCount == 0)
                sample_instr_id = event.instrument_id;

            lobs[event.instrument_id].apply(event);

            processedCount++;

            if (processedCount % 500000 == 0)
            {
                lobs[sample_instr_id].printSnapshot(sample_instr_id, event.timestamp);
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end_time - start_time;

        std::cout << "\n====== PERFORMANCE STATISTICS ======\n";
        std::cout << "Total events processed:   " << processedCount << "\n";
        std::cout << "Processing time:          " << std::fixed << std::setprecision(2) << diff.count() << " sec\n";
        std::cout << "Throughput:               " << static_cast<int>(processedCount / diff.count()) << " events/sec\n";
        std::cout << "Unique Instruments in LOB:" << lobs.size() << "\n\n";

        if (!lobs.empty())
        {
            std::cout << "Final Best Bid/Ask for Instrument " << sample_instr_id << ":\n";
            lobs[sample_instr_id].printBest();
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}