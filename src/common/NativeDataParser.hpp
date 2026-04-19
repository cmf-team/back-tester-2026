#pragma once

#include "BasicTypes.hpp"
#include "MarketDataEvent.hpp"
#include <cstring>
#include <string>
#include <string_view>

namespace cmf {
namespace EventParser {

// Fast native parser that avoids JSON library overhead
class NativeDataParser {
public:
  static bool parse(std::string_view line, MarketDataEvent &event) {
    if (line.empty()) {
      return false;
    }

    // Parse timestamp from "ts_event":"2026-03-09T07:52:41.367824437Z"
    auto ts_pos = line.find("\"ts_event\":\"");
    if (ts_pos != std::string_view::npos) {
      ts_pos += 12; // Skip "ts_event":"
      event.timestamp = parseTimestamp(line.substr(ts_pos, 30));
    }

    // Parse order_id from "order_id":"10996414798222631105"
    auto oid_pos = line.find("\"order_id\":\"");
    if (oid_pos != std::string_view::npos) {
      oid_pos += 12; // Skip "order_id":"
      auto end = line.find('\"', oid_pos);
      if (end != std::string_view::npos) {
        event.order_id = std::string(line.substr(oid_pos, end - oid_pos));
      }
    }

    // Parse side from "side":"B" or "side":"A"
    auto side_pos = line.find("\"side\":\"");
    if (side_pos != std::string_view::npos) {
      char side_char = line[side_pos + 8];
      event.side = parseSideChar(side_char);
    }

    // Parse price from "price":"0.021200000" or "price":null
    auto price_pos = line.find("\"price\":");
    if (price_pos != std::string_view::npos) {
      price_pos += 8; // Skip "price":
      if (line[price_pos] == 'n') { // null
        event.price.reset();
      } else if (line[price_pos] == '\"') {
        price_pos++; // Skip opening quote
        auto end = line.find('\"', price_pos);
        if (end != std::string_view::npos) {
          event.price = parseDouble(line.substr(price_pos, end - price_pos));
        }
      }
    }

    // Parse size from "size":20
    auto size_pos = line.find("\"size\":");
    if (size_pos != std::string_view::npos) {
      size_pos += 7; // Skip "size":
      event.size = parseDouble(line.substr(size_pos, 10));
    }

    // Parse action from "action":"A"
    auto action_pos = line.find("\"action\":\"");
    if (action_pos != std::string_view::npos) {
      char action_char = line[action_pos + 10];
      event.action = parseActionChar(action_char);
    }

    // Parse symbol from "symbol":"EUCO SI 20260710 PS EU P 1.1650 0"
    auto symbol_pos = line.find("\"symbol\":\"");
    if (symbol_pos != std::string_view::npos) {
      symbol_pos += 10; // Skip "symbol":"
      auto end = line.find('\"', symbol_pos);
      if (end != std::string_view::npos) {
        event.symbol = std::string(line.substr(symbol_pos, end - symbol_pos));
      }
    }

    return true;
  }

private:
  static NanoTime parseTimestamp(std::string_view ts_str) {
    // Parse "2026-03-09T07:52:41.367824437Z"
    if (ts_str.length() < 20)
      return 0;

    // Parse YYYY-MM-DD
    int year = parseInt(ts_str.substr(0, 4));
    int month = parseInt(ts_str.substr(5, 2));
    int day = parseInt(ts_str.substr(8, 2));

    // Parse HH:MM:SS
    int hour = parseInt(ts_str.substr(11, 2));
    int minute = parseInt(ts_str.substr(14, 2));
    int second = parseInt(ts_str.substr(17, 2));

    // Calculate seconds since epoch (simplified - doesn't handle all edge cases)
    // Days since 1970-01-01
    int days = (year - 1970) * 365 + (year - 1969) / 4 - (year - 1901) / 100 +
               (year - 1601) / 400;

    // Add days for months
    static const int month_days[] = {0,   31,  59,  90,  120, 151,
                                     181, 212, 243, 273, 304, 334};
    days += month_days[month - 1];
    if (month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
      days++; // Leap year

    days += day - 1;

    long long seconds =
        ((long long)days * 86400LL) + hour * 3600 + minute * 60 + second;

    // Parse fractional seconds (nanoseconds)
    long long nanoseconds = 0;
    auto dot_pos = ts_str.find('.');
    if (dot_pos != std::string_view::npos && dot_pos + 1 < ts_str.length()) {
      auto frac_start = dot_pos + 1;
      auto frac_end = ts_str.find('Z', frac_start);
      if (frac_end == std::string_view::npos)
        frac_end = ts_str.length();

      // Parse up to 9 digits (nanoseconds)
      int digits = 0;
      nanoseconds = 0;
      for (size_t i = frac_start; i < frac_end && digits < 9; i++, digits++) {
        if (ts_str[i] >= '0' && ts_str[i] <= '9') {
          nanoseconds = nanoseconds * 10 + (ts_str[i] - '0');
        }
      }
      // Pad to 9 digits if needed
      while (digits < 9) {
        nanoseconds *= 10;
        digits++;
      }
    }

    return seconds * 1'000'000'000LL + nanoseconds;
  }

  static int parseInt(std::string_view str) {
    int result = 0;
    for (char c : str) {
      if (c >= '0' && c <= '9') {
        result = result * 10 + (c - '0');
      }
    }
    return result;
  }

  static double parseDouble(std::string_view str) {
    double result = 0.0;
    double fraction = 0.0;
    bool in_fraction = false;
    double divisor = 10.0;
    bool negative = false;

    for (char c : str) {
      if (c == '-') {
        negative = true;
      } else if (c >= '0' && c <= '9') {
        if (in_fraction) {
          fraction += (c - '0') / divisor;
          divisor *= 10.0;
        } else {
          result = result * 10.0 + (c - '0');
        }
      } else if (c == '.') {
        in_fraction = true;
      } else if (c == ',' || c == ' ' || c == '}') {
        break;
      }
    }

    result += fraction;
    return negative ? -result : result;
  }

  static Side parseSideChar(char c) {
    switch (c) {
    case 'B':
      return Side::Buy;
    case 'A':
      return Side::Sell;
    default:
      return Side::None;
    }
  }

  static Action parseActionChar(char c) {
    switch (c) {
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
};

} // namespace EventParser
} // namespace cmf
