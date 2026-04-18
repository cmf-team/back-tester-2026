#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "common/Events.hpp"
#include "data_layer/plain_json_parser/PlainJsonParserUtils.hpp"
#include "transport/MarketEventQueue.hpp"

namespace data_layer {
using MarketDataEvent = domain::events::MarketDataEvent;

struct PlainJsonLineParser {
  static void skipWhitespace(const char *&p, const char *const end) noexcept;
  static bool skipToKey(const char *&p, const char *const end,
                          const std::string_view key) noexcept;
  static bool readQuotedView(const char *&p, const char *const end,
                               std::string_view &out) noexcept;
  template <typename IntT>
  static bool readIntegral(const char *&p, const char *const end,
                            IntT &out) noexcept {
    return json_line::readIntegral(p, end, out);
  }
  static std::optional<std::uint64_t>
  parseUtcTimestampNs(const std::string_view timestamp) noexcept;
  static std::optional<std::int64_t>
  parsePrice1e9(const std::string_view price_text) noexcept;
};

template <typename JsonParser = PlainJsonLineParser>
class MarketDataParser final {
public:
  MarketDataParser(
      const std::string &file_path_,
      const transport::MarketEventsQueue::Sptr &market_events_queue_);

  void run();
  void stop() noexcept;

private:
  void parseMboEventsFromFile(const std::string &file_path);

  std::string file_path_;
  transport::MarketEventsQueue::Sptr market_events_queue_;
  std::shared_ptr<std::atomic<bool>> stop_requested_;
};
} // namespace data_layer
