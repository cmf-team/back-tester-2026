#pragma once

#include "BasicTypes.hpp"
#include <nlohmann/json.hpp>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

namespace cmf {
namespace EventParser {

enum class Action {
  None = 0,
  Add,    // A
  Cancel, // C
  Modify, // M
  Remove, // R
  Trade   // T
};

inline Action parseAction(const std::string &str) {
  if (str.empty())
    return Action::None;
  switch (str[0]) {
  case 'A':
    return Action::Add;
  case 'C':
    return Action::Cancel;
  case 'M':
    return Action::Modify;
  case 'R':
    return Action::Remove;
  case 'T':
    return Action::Trade;
  default:
    return Action::None;
  }
}

inline const char *actionToString(Action action) {
  switch (action) {
  case Action::Add:
    return "Add";
  case Action::Cancel:
    return "Cancel";
  case Action::Modify:
    return "Modify";
  case Action::Remove:
    return "Remove";
  case Action::Trade:
    return "Trade";
  default:
    return "None";
  }
}

inline Side parseSide(const std::string &str) {
  if (str.empty())
    return Side::None;
  switch (str[0]) {
  case 'A':
    return Side::Sell;
  case 'B':
    return Side::Buy;
  case 'N':
    return Side::None;
  default:
    return Side::None;
  }
}

inline const char *sideToString(Side side) {
  switch (side) {
  case Side::Buy:
    return "Buy";
  case Side::Sell:
    return "Sell";
  default:
    return "None";
  }
}

class MarketDataEvent {
public:
  NanoTime timestamp;
  std::string order_id;
  Side side;
  std::optional<Price> price;
  Quantity size;
  Action action;
  std::string symbol;

  MarketDataEvent() = default;

  static MarketDataEvent fromJson(const nlohmann::json &j) {
    MarketDataEvent event;

    // Parse timestamp from ts_event in header
    if (j.contains("hd") && j["hd"].contains("ts_event")) {
      std::string ts_str = j["hd"]["ts_event"].get<std::string>();
      event.timestamp = parseTimestamp(ts_str);
    }

    // Parse order_id
    if (j.contains("order_id")) {
      event.order_id = j["order_id"].get<std::string>();
    }

    // Parse side
    if (j.contains("side")) {
      event.side = parseSide(j["side"].get<std::string>());
    }

    // Parse price (can be null)
    if (j.contains("price") && !j["price"].is_null()) {
      event.price = std::stod(j["price"].get<std::string>());
    }

    // Parse size
    if (j.contains("size")) {
      event.size = j["size"].get<double>();
    }

    // Parse action
    if (j.contains("action")) {
      event.action = parseAction(j["action"].get<std::string>());
    }

    // Parse symbol
    if (j.contains("symbol")) {
      event.symbol = j["symbol"].get<std::string>();
    }

    return event;
  }

  void print() const {
    std::cout << "Timestamp: " << timestamp << ", OrderID: " << order_id
              << ", Side: " << sideToString(side) << ", Price: ";
    if (price.has_value()) {
      std::cout << price.value();
    } else {
      std::cout << "null";
    }
    std::cout << ", Size: " << size << ", Action: " << actionToString(action)
              << ", Symbol: " << symbol << std::endl;
  }

private:
  static NanoTime parseTimestamp(const std::string &ts_str) {
    // Parse ISO 8601 timestamp: "2026-03-09T07:52:41.368148840Z"
    // Convert to nanoseconds since epoch
    std::tm tm = {};
    std::istringstream ss(ts_str);

    // Parse date and time
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
      return 0;
    }

    // Get seconds since epoch
    std::time_t seconds = timegm(&tm);

    // Parse fractional seconds
    char dot;
    ss >> dot;
    if (dot == '.') {
      std::string frac_str;
      ss >> frac_str;

      // Remove 'Z' if present
      if (!frac_str.empty() && frac_str.back() == 'Z') {
        frac_str.pop_back();
      }

      // Pad or truncate to 9 digits (nanoseconds)
      if (frac_str.length() < 9) {
        frac_str.append(9 - frac_str.length(), '0');
      } else if (frac_str.length() > 9) {
        frac_str = frac_str.substr(0, 9);
      }

      long nanoseconds = std::stol(frac_str);
      return static_cast<NanoTime>(seconds) * 1'000'000'000 + nanoseconds;
    }

    return static_cast<NanoTime>(seconds) * 1'000'000'000;
  }
};

} // namespace EventParser
} // namespace cmf
