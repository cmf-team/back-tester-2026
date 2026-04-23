#include "common/BasicTypes.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <simdjson.h>
#include <string>
#include <typeinfo>
#include <vector>

using MarketDataEvent = cmf::MarketDataEvent;

const int64_t UNDEF_PRICE = INT64_MAX;
const uint64_t UNDEF_TIMESTAMP = UINT64_MAX;

std::vector<MarketDataEvent> Events;

static uint64_t parseTimestampNanos(const std::string &ts_str) {
  std::chrono::sys_time<std::chrono::nanoseconds> tp;
  std::istringstream ss(ts_str);
  ss >> std::chrono::parse("%Y-%m-%dT%H:%M:%S%Z", tp);
  return static_cast<uint64_t>(tp.time_since_epoch().count());
}

void processMarketDataEvent(const MarketDataEvent &order) {
  if ((int)Events.size() % 200000 == 0) {
    std::cout << order.ts_event << ' ' << order.order_id << ' ' << order.side
              << ' ' << order.price << ' ' << order.size << ' ' << order.action
              << std::endl;
  }
}

int main() {
  // path to .json
  std::string path;
  std::cin >> path;
  auto start = std::chrono::steady_clock::now();
  // parsing NDJSON
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
      event.side = std::string(line["side"].get_string().value());
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
              : std::stof(std::string(price_val.get_string().value()));

      Events.push_back(event);
      processMarketDataEvent(event);
    } catch (...) {
      continue;
    }
  }

  std::cout << "Total messages processed: " << (int)Events.size() << std::endl;
  for (int i = 0; i < 10; ++i) {
    const MarketDataEvent *order = &Events[i];
    std::cout << order->ts_event << ' ' << order->order_id << ' ' << order->side
              << ' ' << order->price << ' ' << order->size << ' '
              << order->action << std::endl;
  }
  for (int i = (int)Events.size() - 1; i > (int)Events.size() - 11; --i) {
    const MarketDataEvent *order = &Events[i];
    std::cout << order->ts_event << ' ' << order->order_id << ' ' << order->side
              << ' ' << order->price << ' ' << order->ts_out << ' '
              << order->size << ' ' << order->action << std::endl;
  }

  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;
  std::cout << "Throughput: " << (int)Events.size() / elapsed.count()
            << " messages per second" << std::endl;
}
