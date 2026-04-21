#include "common/MarketDataEvent.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace cmf;
using json = nlohmann::json;

constexpr int64_t UNDEF_PRICE_INT = 9223372036854775807LL;

uint64_t getSafeUint64(const json &j, const std::string &key) {
  if (!j.contains(key) || j[key].is_null())
    return 0ULL;
  if (j[key].is_string())
    return std::stoull(j[key].get<std::string>());
  return j[key].get<uint64_t>();
}

int64_t getSafeInt64(const json &j, const std::string &key) {
  if (!j.contains(key) || j[key].is_null())
    return 0LL;
  if (j[key].is_string())
    return std::stoll(j[key].get<std::string>());
  return j[key].get<int64_t>();
}

double getSafePrice(const json &j, const std::string &key) {
  if (!j.contains(key) || j[key].is_null())
    return 0.0;
  if (j[key].is_string())
    return std::stod(j[key].get<std::string>());

  if (j[key].is_number_integer()) {
    int64_t raw_price = j[key].get<int64_t>();
    if (raw_price == UNDEF_PRICE_INT)
      return 0.0;
    return static_cast<double>(raw_price) / 1e9;
  }

  return j[key].get<double>();
}

double getSafeDouble(const json &j, const std::string &key) {
  if (!j.contains(key) || j[key].is_null())
    return 0.0;
  if (j[key].is_string())
    return std::stod(j[key].get<std::string>());
  return j[key].get<double>();
}

std::string getSafeString(const json &j, const std::string &key) {
  if (!j.contains(key) || j[key].is_null())
    return "N";
  if (!j[key].is_string())
    return "N";
  return j[key].get<std::string>();
}

Side mapDatabentoSide(const std::string &sideStr) {
  if (sideStr.empty())
    return Side::None;
  if (sideStr[0] == 'A')
    return Side::Sell;
  if (sideStr[0] == 'B')
    return Side::Buy;
  return Side::None;
}

Action mapAction(const std::string &actionStr) {
  if (actionStr.empty())
    return Action::None;
  return static_cast<Action>(actionStr[0]);
}

void processMarketDataEvent(const MarketDataEvent &order) {
  std::cout << "TS: " << order.timestamp << " | OrderID: " << order.order_id
            << " | Side: "
            << (order.side == Side::Buy
                    ? "Buy "
                    : (order.side == Side::Sell ? "Sell" : "None"))
            << " | Price: " << std::fixed << std::setprecision(5) << order.price
            << " | Size: " << order.size
            << " | Action: " << static_cast<char>(order.action) << "\n";
}

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <path_to_ndjson_file>\n";
    return 1;
  }

  std::ifstream file(argv[1]);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << argv[1] << "\n";
    return 1;
  }

  std::string line;
  size_t processedCount = 0;
  NanoTime firstTs = 0;
  NanoTime lastTs = 0;

  std::vector<MarketDataEvent> first10;
  std::vector<MarketDataEvent> last10;

  try {
    while (std::getline(file, line)) {
      if (line.empty())
        continue;

      json j = json::parse(line);
      MarketDataEvent event;

      event.timestamp = getSafeInt64(j, "ts_recv");
      if (event.timestamp == 0) {
        event.timestamp = getSafeInt64(j, "ts_event");
      }

      event.order_id = getSafeUint64(j, "order_id");
      event.price = getSafePrice(j, "price");
      event.size = getSafeDouble(j, "size");
      event.side = mapDatabentoSide(getSafeString(j, "side"));
      event.action = mapAction(getSafeString(j, "action"));

      if (processedCount == 0)
        firstTs = event.timestamp;
      lastTs = event.timestamp;

      if (processedCount < 10) {
        first10.push_back(event);
      }
      if (last10.size() >= 10) {
        last10.erase(last10.begin());
      }
      last10.push_back(event);

      processedCount++;
    }

    std::cout << "\n=== Ingestion Summary ===\n";
    std::cout << "Total messages processed: " << processedCount << "\n";
    std::cout << "First Timestamp: " << firstTs << "\n";
    std::cout << "Last Timestamp:  " << lastTs << "\n\n";

    std::cout << "--- First 10 Events ---\n";
    for (const auto &ev : first10)
      processMarketDataEvent(ev);

    std::cout << "\n--- Last 10 Events ---\n";
    for (const auto &ev : last10)
      processMarketDataEvent(ev);

  } catch (const std::exception &ex) {
    std::cerr << "Back-tester threw an exception during ingestion: "
              << ex.what() << std::endl;
    return 1;
  }

  return 0;
}