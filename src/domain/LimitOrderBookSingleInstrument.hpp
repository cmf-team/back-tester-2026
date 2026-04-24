#pragma once

#include "common/BasicTypes.hpp"
#include "common/Events.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

namespace domain {
using namespace cmf;
using MarketDataEvent = events::MarketDataEvent;
using InstrumentId = decltype(std::declval<MarketDataEvent>().hd.instrument_id);

using BidsBookMap = std::map<Price, Quantity, std::greater<Price>>;
using AsksBookMap = std::map<Price, Quantity, std::less<Price>>;

using PriceLevel = std::pair<Price, Quantity>;
using BestQuote = std::optional<PriceLevel>;


struct ExecStats {
  std::uint64_t count{0};
  Quantity total_size{0};
  std::optional<Price> last_price{};
  Quantity last_size{0};
  char last_side{0};
};

class LimitOrderBookSingleInstrument final {
public:
  using Sptr = std::shared_ptr<LimitOrderBookSingleInstrument>;

  struct OrderState {
    char side{};
    Price price{};
    Quantity size{};
  };

  void onAdd(const MarketDataEvent &event);
  void onModify(const MarketDataEvent &event);
  void onCancel(const MarketDataEvent &event);
  void onTrade(const MarketDataEvent &event);
  void onFill(const MarketDataEvent &event);
  void onClear();
  void onEvent(const MarketDataEvent &event);

  BidsBookMap getBids() const;
  AsksBookMap getAsks() const;
  BestQuote getBestBid() const;
  BestQuote getBestAsk() const;
  Quantity getVolumeAtPrice(char side, Price price) const;

  ExecStats getTradeStats() const;
  ExecStats getFillStats() const;

private:
  static bool isBookSide(char side);
  void addOrderContribution(const OrderState &state);
  void removeOrderContribution(const OrderState &state);
  void adjustLevel(char side, Price price, Quantity delta);
  static void updateExecStats(ExecStats &stats, const MarketDataEvent &event);

  mutable std::mutex m_;
  BidsBookMap bids_;
  AsksBookMap asks_;
  std::unordered_map<OrderId, OrderState> orders_;

  ExecStats trade_stats_{};
  ExecStats fill_stats_{};
};
} 
