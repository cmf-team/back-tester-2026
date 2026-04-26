// Per-instrument L2 order book rebuilt from Databento MBO events.

#pragma once

#include "common/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

namespace cmf {

struct PriceLevel {
  std::int64_t price{UNDEF_PRICE};
  std::uint64_t size{0};
};

struct ApplyResult {
  bool missing_order = false;
  bool ignored = false;
};

class LimitOrderBook {
public:
  explicit LimitOrderBook(std::uint32_t instrument_id);

  ApplyResult apply(const MarketDataEvent &event);

  [[nodiscard]] std::optional<PriceLevel> bestBid() const noexcept;
  [[nodiscard]] std::optional<PriceLevel> bestAsk() const noexcept;
  [[nodiscard]] std::uint64_t volumeAtPrice(MdSide side,
                                            std::int64_t price) const noexcept;
  [[nodiscard]] std::size_t orderCount() const noexcept { return orders_.size(); }
  [[nodiscard]] std::size_t bidLevelCount() const noexcept { return bids_.size(); }
  [[nodiscard]] std::size_t askLevelCount() const noexcept { return asks_.size(); }
  [[nodiscard]] bool hasOrder(std::uint16_t publisher_id,
                              std::uint64_t order_id) const noexcept;
  [[nodiscard]] std::uint32_t instrumentId() const noexcept {
    return instrument_id_;
  }
  [[nodiscard]] std::string snapshotString(std::size_t depth = 5) const;

private:
  struct OrderKey {
    std::uint16_t publisher_id{0};
    std::uint64_t order_id{0};

    bool operator==(const OrderKey &) const = default;
  };

  struct OrderKeyHash {
    std::size_t operator()(const OrderKey &key) const noexcept;
  };

  struct OrderState {
    std::int64_t price{UNDEF_PRICE};
    std::uint32_t size{0};
    MdSide side{MdSide::None};
  };

  ApplyResult applyAdd(const MarketDataEvent &event);
  ApplyResult applyCancel(const MarketDataEvent &event);
  ApplyResult applyModify(const MarketDataEvent &event);
  void addOrder(const OrderKey &key, const OrderState &state);
  void eraseOrder(const OrderKey &key, const OrderState &state);
  static bool isRestingOrder(MdSide side, std::int64_t price,
                             std::uint32_t size) noexcept;
  static OrderKey makeOrderKey(const MarketDataEvent &event) noexcept;

  std::uint32_t instrument_id_{0};
  std::map<std::int64_t, std::uint64_t, std::greater<>> bids_;
  std::map<std::int64_t, std::uint64_t> asks_;
  std::unordered_map<OrderKey, OrderState, OrderKeyHash> orders_;
};

} // namespace cmf
