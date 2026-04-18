#pragma once

#include "common/MarketDataEvent.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace cmf {

// Callback type: receives each parsed event.
// The dispatcher (caller) decides what to do — print, enqueue, update LOB, etc.
using EventCallback = std::function<void(const MarketDataEvent &)>;

// Parses Databento NDJSON L3 files into MarketDataEvent objects.
//
// Usage:
//   MarketDataParser parser;
//   parser.parseFile("path/to/daily.mbo.json", [](const MarketDataEvent& e) {
//       processMarketDataEvent(e);
//   });
//
class MarketDataParser {
public:
  MarketDataParser() = default;

  // Parse a single daily NDJSON file, invoking callback for every valid event.
  // Returns the total number of events successfully parsed.
  std::size_t parseFile(const std::string &filePath, EventCallback callback);

  // Parse one NDJSON line into a MarketDataEvent.
  // Returns true on success, false if the line is malformed or empty.
  static bool parseLine(std::string_view line, MarketDataEvent &outEvent);

private:
  // Field-level helpers — each extracts one piece from the JSON line.
  static NanoTime parseTimestamp(std::string_view tsStr);
  static Action parseAction(char ch);
  static Side parseSide(char ch);
  static Price parsePrice(std::string_view priceStr);
  static OrderId parseOrderId(std::string_view idStr);
};

} // namespace cmf
