#include "main/ingestion/MarketDataParser.hpp"

#include <charconv>
#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace cmf {

std::size_t MarketDataParser::parseFile(const std::string &filePath,
                                        EventCallback callback) {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    return 0;
  }

  std::size_t count = 0;
  std::string line;
  MarketDataEvent event;

  while (std::getline(file, line)) {
    if (parseLine(line, event)) {
      callback(event);
      ++count;
    }
  }

  return count;
}

bool MarketDataParser::parseLine(std::string_view line,
                                 MarketDataEvent &outEvent) {
  if (line.empty() ||
      line.find_first_not_of(" \t\r\n") == std::string_view::npos) {
    return false;
  }

  nlohmann::json j;
  try {
    j = nlohmann::json::parse(line);
  } catch (const nlohmann::json::parse_error &) {
    return false;
  }

  // action is required
  if (!j.contains("action") || !j["action"].is_string()) {
    return false;
  }

  const auto &hd = j["hd"];

  outEvent.tsRecv = parseTimestamp(j["ts_recv"].get<std::string_view>());
  outEvent.tsEvent = parseTimestamp(hd["ts_event"].get<std::string_view>());

  outEvent.action = parseAction(j["action"].get<std::string_view>()[0]);
  outEvent.side = parseSide(j["side"].get<std::string_view>()[0]);

  if (j["price"].is_null()) {
    outEvent.price = 0.0;
  } else {
    outEvent.price = parsePrice(j["price"].get<std::string_view>());
  }

  outEvent.size = j["size"].get<double>();
  outEvent.orderId = parseOrderId(j["order_id"].get<std::string_view>());

  outEvent.instrumentId = hd["instrument_id"].get<std::uint32_t>();
  outEvent.channelId = j["channel_id"].get<std::uint16_t>();
  outEvent.publisherId = hd["publisher_id"].get<std::uint16_t>();
  outEvent.flags = j["flags"].get<std::uint8_t>();
  outEvent.rtype = hd["rtype"].get<std::uint8_t>();

  outEvent.tsInDelta = j["ts_in_delta"].get<std::int32_t>();
  outEvent.sequence = j["sequence"].get<std::uint32_t>();

  outEvent.symbol = j["symbol"].get<std::string>();

  return true;
}

// Parses "2026-03-09T00:03:00.129732099Z" into nanoseconds since UNIX epoch.
NanoTime MarketDataParser::parseTimestamp(std::string_view tsStr) {
  // Parse: YYYY-MM-DDThh:mm:ss.nnnnnnnnnZ
  std::tm tm{};
  tm.tm_year = (tsStr[0] - '0') * 1000 + (tsStr[1] - '0') * 100 +
               (tsStr[2] - '0') * 10 + (tsStr[3] - '0') - 1900;
  tm.tm_mon = (tsStr[5] - '0') * 10 + (tsStr[6] - '0') - 1;
  tm.tm_mday = (tsStr[8] - '0') * 10 + (tsStr[9] - '0');
  tm.tm_hour = (tsStr[11] - '0') * 10 + (tsStr[12] - '0');
  tm.tm_min = (tsStr[14] - '0') * 10 + (tsStr[15] - '0');
  tm.tm_sec = (tsStr[17] - '0') * 10 + (tsStr[18] - '0');

  std::time_t epochSecs = timegm(&tm);

  // Parse the fractional seconds (up to 9 digits after the '.')
  NanoTime nanos = 0;
  if (tsStr.size() > 20 && tsStr[19] == '.') {
    auto fracStart = tsStr.data() + 20;
    // Find end of digits (before 'Z')
    auto fracEnd = tsStr.data() + tsStr.size();
    if (tsStr.back() == 'Z') {
      --fracEnd;
    }
    std::size_t digits = static_cast<std::size_t>(fracEnd - fracStart);

    std::int64_t frac = 0;
    std::from_chars(fracStart, fracEnd, frac);

    // Pad to 9 digits: if fewer digits, multiply up
    for (std::size_t i = digits; i < 9; ++i) {
      frac *= 10;
    }
    nanos = frac;
  }

  return static_cast<NanoTime>(epochSecs) * 1'000'000'000LL + nanos;
}

Action MarketDataParser::parseAction(char ch) {
  switch (ch) {
  case 'A':
    return Action::Add;
  case 'M':
    return Action::Modify;
  case 'C':
    return Action::Cancel;
  case 'R':
    return Action::Clear;
  case 'T':
    return Action::Trade;
  case 'F':
    return Action::Fill;
  default:
    return Action::None;
  }
}

Side MarketDataParser::parseSide(char ch) {
  switch (ch) {
  case 'B':
    return Side::Buy;
  case 'A':
    return Side::Sell;
  default:
    return Side::None;
  }
}

Price MarketDataParser::parsePrice(std::string_view priceStr) {
  double val = 0.0;
  std::from_chars(priceStr.data(), priceStr.data() + priceStr.size(), val);
  return val;
}

OrderId MarketDataParser::parseOrderId(std::string_view idStr) {
  std::uint64_t val = 0;
  std::from_chars(idStr.data(), idStr.data() + idStr.size(), val);
  return val;
}

} // namespace cmf
