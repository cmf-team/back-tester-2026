#include "ParserUtils.hpp"

#include <chrono>
#include <simdjson.h>
#include <sstream>
#include <string>
#include <string_view>

namespace cmf {

namespace {

Side parseSide(std::string_view s) {
  if (s == "B")
    return Side::Buy;
  if (s == "A")
    return Side::Sell;
  return Side::None;
}

uint64_t parseTimestampNanos(const std::string &ts_str) {
  std::chrono::sys_time<std::chrono::nanoseconds> tp;
  std::istringstream ss(ts_str);
  ss >> std::chrono::parse("%Y-%m-%dT%H:%M:%S%Z", tp);
  return static_cast<uint64_t>(tp.time_since_epoch().count());
}

} // namespace

void parseNdjsonFile(const std::string &path,
                     std::vector<MarketDataEvent> &events,
                     const MarketDataEventCallback &onEvent) {
  simdjson::ondemand::parser parser;
  simdjson::padded_string json = simdjson::padded_string::load(path);
  simdjson::ondemand::document_stream lines = parser.iterate_many(json);

  for (auto line : lines) {
    try {
      MarketDataEvent event;
      auto parseOptTs =
          [&](simdjson::simdjson_result<std::string_view> res) -> uint64_t {
        return res.error() ? UNDEF_TIMESTAMP
                           : parseTimestampNanos(std::string(res.value()));
      };
      event.ts_recv = parseOptTs(line["ts_recv"].get_string());
      event.ts_event = parseOptTs(line["hd"]["ts_event"].get_string());
      event.ts_out = parseOptTs(line["ts_out"].get_string());

      event.action = std::string(line["action"].get_string().value());
      event.side = parseSide(line["side"].get_string().value());
      event.symbol = std::string(line["symbol"].get_string().value());
      event.instrument_id = line["hd"]["instrument_id"].get_uint64();
      event.rtype = line["hd"]["rtype"].get_int64();
      event.publisher_id = line["hd"]["publisher_id"].get_int64();
      event.ts_in_delta = static_cast<int32_t>(line["ts_in_delta"].get_int64());
      event.sequence = line["sequence"].get_int64();
      event.size = line["size"].get_int64();
      event.flags = line["flags"].get_uint64();
      event.channel_id = line["channel_id"].get_uint64();
      event.order_id =
          std::stoull(std::string(line["order_id"].get_string().value()));
      auto price_val = line["price"];
      event.price =
          price_val.is_null()
              ? UNDEF_PRICE
              : static_cast<int64_t>(
                    std::stod(std::string(price_val.get_string().value())) *
                    1e9);

      events.push_back(event);
      if (onEvent) {
        onEvent(events.back());
      }
    } catch (...) {
      continue;
    }
  }
}

} // namespace cmf
