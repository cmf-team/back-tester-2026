// defines basic types used throughout the code
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace cmf
{
  using NanoTime = std::int64_t;

  using ClOrdId = std::uint64_t;
  using OrderId = std::uint64_t;
  using StrategyId = std::uint16_t;
  using MarketId = std::uint16_t;
  using SecurityId = std::uint16_t;

  using Quantity = double;
  using Price = double;

  enum class Side : signed short { None = 0, Buy = 1, Sell = -1 };

  enum class OrderType { None = 0, Limit, Market };

  enum class TimeInForce { None = 0, GoodTillCancel, FillAndKill, FillOrKill };

  enum class SecurityType { None = 0, FX, Stock, Bond, Future, Option };

  struct MarketSecurityId {
    MarketId mktId;
    SecurityId secId;
    bool operator==(const MarketSecurityId& other) const noexcept = default;
  };

  struct MarketSecurityIdHash {
    std::size_t operator()(const MarketSecurityId& key) const noexcept {
      const std::size_t h1 = std::hash<SecurityId>{}(key.secId);
      const std::size_t h2 = std::hash<MarketId>{}(key.mktId);
      return h1 ^ (h2 << 1);
    }
  };

  class MktId {
  public:
    static constexpr MarketId None = 0;
  };

  struct SecId {
    static constexpr SecurityId None = 0;
  };

  struct MktSecId {
    static constexpr MarketSecurityId None = {0, 0};
  };

} // namespace cmf