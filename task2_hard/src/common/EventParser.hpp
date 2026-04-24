#pragma once
#include <string>
#include <vector>
#include "MarketDataEvent.hpp"

// Parse a Databento NDJSON MBO file into a vector of MarketDataEvents.
// Uses simdjson on_demand API for maximum throughput (~2 GB/s).
// Each line is one JSON object; lines that cannot be parsed are skipped.
std::vector<MarketDataEvent> parse_file(const std::string& path);

// Parse a single JSON line (exposed for unit testing)
bool parse_line(std::string_view line, MarketDataEvent& out);
