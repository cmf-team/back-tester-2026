#include "MarketDataParser.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <string>

#include <chrono>
#include <sstream>

using json = nlohmann::json;

namespace {

std::uint64_t getTimestampOrDefault(const json &j, const char *key) {
  if (!j.contains(key) || j.at(key).is_null()) {
    return MarketDataEvent::UNDEF_TIMESTAMP;
  }

  if (j.at(key).is_number_unsigned()) {
    return j.at(key).get<std::uint64_t>();
  }

  if (j.at(key).is_number_integer()) {
    const auto value = j.at(key).get<std::int64_t>();
    return value >= 0 ? static_cast<std::uint64_t>(value)
                      : MarketDataEvent::UNDEF_TIMESTAMP;
  }

  if (j.at(key).is_string()) {
    return static_cast<std::uint64_t>(
        std::stoull(j.at(key).get<std::string>()));
  }

  return MarketDataEvent::UNDEF_TIMESTAMP;
}

std::int32_t getInt32OrDefault(const json &j, const char *key,
                               std::int32_t fallback = 0) {
  if (!j.contains(key) || j.at(key).is_null()) {
    return fallback;
  }
  return j.at(key).get<std::int32_t>();
}

std::uint32_t getUInt32OrDefault(const json &j, const char *key,
                                 std::uint32_t fallback = 0) {
  if (!j.contains(key) || j.at(key).is_null()) {
    return fallback;
  }
  return j.at(key).get<std::uint32_t>();
}

std::uint64_t getUInt64OrDefault(const json &j, const char *key,
                                 std::uint64_t fallback = 0) {
  if (!j.contains(key) || j.at(key).is_null()) {
    return fallback;
  }
  return j.at(key).get<std::uint64_t>();
}

std::int64_t getPriceOrDefault(const json &j, const char *key) {
  if (!j.contains(key) || j.at(key).is_null()) {
    return MarketDataEvent::UNDEF_PRICE;
  }

  if (j.at(key).is_number_integer()) {
    return j.at(key).get<std::int64_t>();
  }

  if (j.at(key).is_number_float()) {
    const double px = j.at(key).get<double>();
    return static_cast<std::int64_t>(std::llround(px * 1'000'000'000.0));
  }

  if (j.at(key).is_string()) {
    const double px = std::stod(j.at(key).get<std::string>());
    return static_cast<std::int64_t>(std::llround(px * 1'000'000'000.0));
  }

  return MarketDataEvent::UNDEF_PRICE;
}

MarketDataEvent::Side parseSide(const json &j) {
  if (!j.contains("side") || j.at("side").is_null()) {
    return MarketDataEvent::Side::None;
  }

  const std::string side = j.at("side").get<std::string>();
  if (side == "B")
    return MarketDataEvent::Side::Bid;
  if (side == "A")
    return MarketDataEvent::Side::Ask;
  return MarketDataEvent::Side::None;
}

MarketDataEvent::Action parseAction(const json &j) {
  if (!j.contains("action") || j.at("action").is_null()) {
    return MarketDataEvent::Action::None;
  }

  const std::string action = j.at("action").get<std::string>();
  if (action == "A")
    return MarketDataEvent::Action::Add;
  if (action == "M")
    return MarketDataEvent::Action::Modify;
  if (action == "C")
    return MarketDataEvent::Action::Cancel;
  if (action == "R")
    return MarketDataEvent::Action::Clear;
  if (action == "T")
    return MarketDataEvent::Action::Trade;
  if (action == "F")
    return MarketDataEvent::Action::Fill;
  return MarketDataEvent::Action::None;
}

} // namespace

// Helper to convert ISO 8601 string to nanoseconds since epoch
uint64_t isoToNanos(const std::string &iso_str) {
  if (iso_str.empty())
    return 0;
  std::tm tm = {};
  std::istringstream ss(iso_str);
  ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

  // Extract fractional part manually
  auto dot = iso_str.find('.');
  auto z = iso_str.find('Z');
  uint64_t nanos = 0;
  if (dot != std::string::npos && z != std::string::npos) {
    std::string frac = iso_str.substr(dot + 1, z - dot - 1);
    while (frac.size() < 9)
      frac += '0'; // Pad to nanos
    nanos = std::stoull(frac.substr(0, 9));
  }
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             tp.time_since_epoch())
             .count() +
         nanos;
}

std::optional<MarketDataEvent> parseLine(const std::string &line) {
  try {
    auto j = json::parse(line);
    auto hd = j.at("hd");

    // Databento Rule: sort_ts = ts_recv (if exists) else ts_event
    uint64_t recv_ns = isoToNanos(j.value("ts_recv", ""));
    uint64_t event_ns = isoToNanos(hd.value("ts_event", ""));
    uint64_t final_sort_ts = (recv_ns != 0) ? recv_ns : event_ns;

    return MarketDataEvent(
        final_sort_ts, recv_ns, event_ns, j.value("ts_in_delta", 0),
        hd.value("publisher_id", 0u), hd.value("instrument_id", 0u),
        std::stoull(
            j.at("order_id").get<std::string>()), // Handle "order_id" as string
        static_cast<int64_t>(std::stod(j.at("price").get<std::string>()) *
                             1e9), // Decimal to fixed-point
        j.value("size", 0u),
        (j.at("side").get<std::string>() == "B" ? MarketDataEvent::Side::Bid
                                                : MarketDataEvent::Side::Ask),
        MarketDataEvent::Action::Add, // map j.at("action") here
        j.value("flags", 0), hd.value("rtype", 0), 0);
  } catch (...) {
    return std::nullopt;
  }
}