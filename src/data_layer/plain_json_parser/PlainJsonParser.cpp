#include "data_layer/plain_json_parser/PlainJsonParser.hpp"

#include <atomic>
#include <charconv>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace data_layer {

void PlainJsonLineParser::skipWhitespace(const char *&p,
                                          const char *const end) noexcept {
  json_line::skipWhitespace(p, end);
}

bool PlainJsonLineParser::skipToKey(const char *&p, const char *const end,
                                      const std::string_view key) noexcept {
  return json_line::skipToKey(p, end, key);
}

bool PlainJsonLineParser::readQuotedView(const char *&p,
                                           const char *const end,
                                           std::string_view &out) noexcept {
  return json_line::readQuotedView(p, end, out);
}

std::optional<std::uint64_t> PlainJsonLineParser::parseUtcTimestampNs(
    const std::string_view timestamp) noexcept {
  return json_line::parseUtcTimestampNs(timestamp);
}

std::optional<std::int64_t> PlainJsonLineParser::parsePrice1e9(
    const std::string_view price_text) noexcept {
  return json_line::parsePrice1e9(price_text);
}

namespace {

template <typename JsonParser>
std::optional<MarketDataEvent> parseMboLine(const std::string_view line) {
  if (line.empty() || line.front() != '{') {
    return std::nullopt;
  }

  const char *p = line.data();
  const char *const end = p + line.size();
  MarketDataEvent event{};

  std::string_view token{};

  if (!JsonParser::skipToKey(p, end, R"("ts_recv":)") ||
      !JsonParser::readQuotedView(p, end, token)) {
    return std::nullopt;
  }
  event.ts_recv.assign(token.data(), token.size());

  if (!JsonParser::skipToKey(p, end, R"("ts_event":)") ||
      !JsonParser::readQuotedView(p, end, token)) {
    return std::nullopt;
  }
  if (const auto ts = JsonParser::parseUtcTimestampNs(token)) {
    event.hd.ts_event = *ts;
  } else {
    return std::nullopt;
  }

  if (!JsonParser::skipToKey(p, end, R"("rtype":)") ||
      !JsonParser::readIntegral(p, end, event.hd.rtype)) {
    return std::nullopt;
  }
  if (!JsonParser::skipToKey(p, end, R"("publisher_id":)") ||
      !JsonParser::readIntegral(p, end, event.hd.publisher_id)) {
    return std::nullopt;
  }
  if (!JsonParser::skipToKey(p, end, R"("instrument_id":)") ||
      !JsonParser::readIntegral(p, end, event.hd.instrument_id)) {
    return std::nullopt;
  }

  if (!JsonParser::skipToKey(p, end, R"("action":)") ||
      !JsonParser::readQuotedView(p, end, token) || token.empty()) {
    return std::nullopt;
  }
  event.action = token.front();

  if (!JsonParser::skipToKey(p, end, R"("side":)") ||
      !JsonParser::readQuotedView(p, end, token) || token.empty()) {
    return std::nullopt;
  }
  event.side = token.front();

  if (!JsonParser::skipToKey(p, end, R"("price":)")) {
    return std::nullopt;
  }
  JsonParser::skipWhitespace(p, end);
  if (p + 4 <= end && std::strncmp(p, "null", 4) == 0) {
    event.price = domain::events::UNDEF_PRICE;
    p += 4;
  } else {
    if (!JsonParser::readQuotedView(p, end, token)) {
      return std::nullopt;
    }
    if (const auto price = JsonParser::parsePrice1e9(token)) {
      event.price = *price;
    } else {
      return std::nullopt;
    }
  }

  if (!JsonParser::skipToKey(p, end, R"("size":)") ||
      !JsonParser::readIntegral(p, end, event.size)) {
    return std::nullopt;
  }
  if (!JsonParser::skipToKey(p, end, R"("channel_id":)") ||
      !JsonParser::readIntegral(p, end, event.channel_id)) {
    return std::nullopt;
  }

  if (!JsonParser::skipToKey(p, end, R"("order_id":)") ||
      !JsonParser::readQuotedView(p, end, token)) {
    return std::nullopt;
  }
  {
    const char *order_begin = token.data();
    const char *order_end = order_begin + token.size();
    const auto [ptr, ec] =
        std::from_chars(order_begin, order_end, event.order_id);
    if (ec != std::errc{} || ptr != order_end) {
      return std::nullopt;
    }
  }

  if (!JsonParser::skipToKey(p, end, R"("flags":)") ||
      !JsonParser::readIntegral(p, end, event.flags)) {
    return std::nullopt;
  }
  if (!JsonParser::skipToKey(p, end, R"("ts_in_delta":)") ||
      !JsonParser::readIntegral(p, end, event.ts_in_delta)) {
    return std::nullopt;
  }
  if (!JsonParser::skipToKey(p, end, R"("sequence":)") ||
      !JsonParser::readIntegral(p, end, event.sequence)) {
    return std::nullopt;
  }

  if (!JsonParser::skipToKey(p, end, R"("symbol":)") ||
      !JsonParser::readQuotedView(p, end, token)) {
    return std::nullopt;
  }
  event.symbol.assign(token.data(), token.size());
  return event;
}

} 

template <typename JsonParser>
MarketDataParser<JsonParser>::MarketDataParser(
    const std::string &file_path_,
    const transport::MarketEventsQueue::Sptr &market_events_queue_)
    : file_path_(file_path_), market_events_queue_(market_events_queue_),
      stop_requested_(std::make_shared<std::atomic<bool>>(false)) {}

template <typename JsonParser> void MarketDataParser<JsonParser>::run() {
  stop_requested_->store(false, std::memory_order_release);
  parseMboEventsFromFile(file_path_);
}

template <typename JsonParser>
void MarketDataParser<JsonParser>::stop() noexcept {
  stop_requested_->store(true, std::memory_order_release);
}

template <typename JsonParser>
void MarketDataParser<JsonParser>::parseMboEventsFromFile(
    const std::string &file_path) {
  constexpr std::size_t k_stream_buf_bytes = 1U << 20;
  std::vector<char> stream_buf(k_stream_buf_bytes);

  std::ifstream input;
  input.rdbuf()->pubsetbuf(stream_buf.data(),
                           static_cast<std::streamsize>(stream_buf.size()));
  input.open(file_path);
  if (!input.is_open()) {
    throw std::runtime_error("Could not open JSON file: " + file_path);
  }

  std::string line;
  std::size_t line_no = 0;
  line.reserve(4096);
  while (std::getline(input, line)) {
    if (stop_requested_->load(std::memory_order_acquire)) {
      market_events_queue_->put(domain::events::EOF_EVENT);
      return;
    }
    ++line_no;
    if (json_line::isBlankLine(line)) {
      continue;
    }
    const auto event = parseMboLine<JsonParser>(line);
    if (!event.has_value()) {
      throw std::runtime_error("Failed to parse NDJSON line in file: " +
                               file_path + " line: " + std::to_string(line_no));
    }
    market_events_queue_->put(*event);
  }

  market_events_queue_->put(domain::events::EOF_EVENT);
}

template class MarketDataParser<PlainJsonLineParser>;

} 
