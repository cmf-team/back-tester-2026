

#include "common/EventParser.hpp"

#include <nlohmann/json.hpp>

#include <charconv>
#include <chrono>
#include <ctime>
#include <stdexcept>

namespace cmf {

namespace {




NanoTime isoToNanos(std::string_view s) {


  if (s.size() < 19) {
    return 0;
  }

  auto toInt = [](std::string_view sv) -> int {
    int v = 0;
    std::from_chars(sv.data(), sv.data() + sv.size(), v);
    return v;
  };

  std::tm tm{};
  tm.tm_year = toInt(s.substr(0, 4)) - 1900;
  tm.tm_mon  = toInt(s.substr(5, 2)) - 1;
  tm.tm_mday = toInt(s.substr(8, 2));
  tm.tm_hour = toInt(s.substr(11, 2));
  tm.tm_min  = toInt(s.substr(14, 2));
  tm.tm_sec  = toInt(s.substr(17, 2));




  using namespace std::chrono;
  year_month_day ymd{
      year{tm.tm_year + 1900},
      month{static_cast<unsigned>(tm.tm_mon + 1)},
      day{static_cast<unsigned>(tm.tm_mday)}};
  sys_days days = ymd;
  auto sec_since_epoch =
      duration_cast<seconds>(days.time_since_epoch()).count() +
      tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;


  std::int64_t nanos = 0;
  if (s.size() > 20 && s[19] == '.') {

    std::size_t end = 20;
    while (end < s.size() && s[end] >= '0' && s[end] <= '9') {
      ++end;
    }
    auto frac = s.substr(20, end - 20);
    int v = 0;
    std::from_chars(frac.data(), frac.data() + frac.size(), v);

    int len = static_cast<int>(frac.size());
    static constexpr std::int64_t pow10[] = {
        1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
    if (len <= 9) {
      nanos = static_cast<std::int64_t>(v) * pow10[9 - len];
    }
  }

  return static_cast<NanoTime>(sec_since_epoch) * 1'000'000'000LL + nanos;
}

} // namespace

NanoTime parseIsoTimestamp(std::string_view iso) {
  return isoToNanos(iso);
}

std::int64_t parseFixedPrice(std::string_view str) {
  if (str.empty() || str == "null") {
    return MarketDataEvent::UNDEF_PRICE;
  }


  auto dot = str.find('.');
  std::int64_t whole = 0;
  std::int64_t frac  = 0;
  int frac_len       = 0;

  if (dot == std::string_view::npos) {
    std::from_chars(str.data(), str.data() + str.size(), whole);
  } else {
    std::from_chars(str.data(), str.data() + dot, whole);
    auto frac_sv = str.substr(dot + 1);
    std::from_chars(frac_sv.data(), frac_sv.data() + frac_sv.size(), frac);
    frac_len = static_cast<int>(frac_sv.size());
  }


  static constexpr std::int64_t pow10[] = {
      1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
  std::int64_t scale =
      (frac_len <= 9) ? pow10[9 - frac_len] : 1;

  std::int64_t sign = (whole < 0) ? -1 : 1;
  return sign * (std::abs(whole) * 1'000'000'000LL + frac * scale);
}

std::optional<MarketDataEvent> parseEvent(std::string_view line) {

  if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string_view::npos) {
    return std::nullopt;
  }

  using nlohmann::json;
  json j;
  try {
    j = json::parse(line);
  } catch (const json::parse_error &) {
    return std::nullopt;
  }

  MarketDataEvent ev;


  if (j.contains("ts_recv") && j["ts_recv"].is_string()) {
    ev.ts_recv = isoToNanos(j["ts_recv"].get<std::string>());
  }


  if (j.contains("hd") && j["hd"].is_object()) {
    const auto &hd = j["hd"];
    if (hd.contains("ts_event") && hd["ts_event"].is_string()) {
      ev.ts_event = isoToNanos(hd["ts_event"].get<std::string>());
    }
    if (hd.contains("publisher_id")) {
      ev.publisher_id = hd["publisher_id"].get<std::uint16_t>();
    }
    if (hd.contains("instrument_id")) {
      ev.instrument_id = hd["instrument_id"].get<std::uint32_t>();
    }
  }

  // action
  if (j.contains("action") && j["action"].is_string()) {
    auto s = j["action"].get<std::string>();
    if (!s.empty()) {
      ev.action = static_cast<Action>(s[0]);
    }
  }

  // side
  if (j.contains("side") && j["side"].is_string()) {
    auto s = j["side"].get<std::string>();
    if (!s.empty()) {
      ev.side = static_cast<SideChar>(s[0]);
    }
  }


  if (j.contains("price") && !j["price"].is_null()) {
    if (j["price"].is_string()) {
      ev.price = parseFixedPrice(j["price"].get<std::string>());
    } else if (j["price"].is_number()) {

      ev.price = j["price"].get<std::int64_t>();
    }
  }

  // size
  if (j.contains("size") && j["size"].is_number()) {
    ev.size = j["size"].get<std::int64_t>();
  }


  if (j.contains("order_id") && j["order_id"].is_string()) {
    auto s = j["order_id"].get<std::string>();
    std::from_chars(s.data(), s.data() + s.size(), ev.order_id);
  }

  if (j.contains("flags") && j["flags"].is_number()) {
    ev.flags = j["flags"].get<std::uint8_t>();
  }
  if (j.contains("ts_in_delta") && j["ts_in_delta"].is_number()) {
    ev.ts_in_delta = j["ts_in_delta"].get<std::int32_t>();
  }
  if (j.contains("sequence") && j["sequence"].is_number()) {
    ev.sequence = j["sequence"].get<std::uint32_t>();
  }
  if (j.contains("symbol") && j["symbol"].is_string()) {
    ev.symbol = j["symbol"].get<std::string>();
  }

  return ev;
}

} // namespace cmf
