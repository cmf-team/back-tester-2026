// Databento MBO (L3) record, value type fed to processMarketDataEvent().
// Comparator used by the hard-variant k-way merger; unused in easy path.

#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>
#include <limits>

namespace cmf {

inline constexpr std::uint64_t UNDEF_TIMESTAMP =
    std::numeric_limits<std::uint64_t>::max();
inline constexpr std::int64_t UNDEF_PRICE =
    std::numeric_limits<std::int64_t>::max();

enum class MdAction : char {
  None = 'N',
  Add = 'A',
  Modify = 'M',
  Cancel = 'C',
  Clear = 'R',
  Trade = 'T',
  Fill = 'F',
};

enum class MdSide : char {
  None = 'N',
  Ask = 'A',
  Bid = 'B',
};

// Databento rtype for MBO records.
inline constexpr std::uint8_t RTYPE_MBO = 0xA0;

struct MarketDataEvent {
  std::uint64_t ts_recv{UNDEF_TIMESTAMP};
  std::uint64_t ts_event{UNDEF_TIMESTAMP};
  std::int32_t ts_in_delta{0};
  std::uint16_t publisher_id{0};
  std::uint32_t instrument_id{0};
  std::uint64_t order_id{0};
  std::int64_t price{UNDEF_PRICE}; // 1e-9 fixed precision
  std::uint32_t size{0};
  std::uint32_t sequence{0};
  std::uint8_t channel_id{0};
  std::uint8_t flags{0};
  MdAction action{MdAction::None};
  MdSide side{MdSide::None};
};

// Primary ordering matches the Databento "index timestamp" convention and the
// future k-way merger. Tie-break by publisher_id then sequence so the order is
// total (safe for priority_queue).
constexpr auto mdeOrderKey(const MarketDataEvent &e) noexcept {
  struct Key {
    std::uint64_t ts_recv;
    std::uint16_t publisher_id;
    std::uint32_t sequence;
    auto operator<=>(const Key &) const = default;
  };
  return Key{e.ts_recv, e.publisher_id, e.sequence};
}

constexpr std::strong_ordering operator<=>(const MarketDataEvent &a,
                                           const MarketDataEvent &b) noexcept {
  return mdeOrderKey(a) <=> mdeOrderKey(b);
}

constexpr bool sameOrderKey(const MarketDataEvent &a,
                            const MarketDataEvent &b) noexcept {
  return mdeOrderKey(a) == mdeOrderKey(b);
}

// Print the spec-required fields in a one-line, space-separated format.
// Format is exercised by both processMarketDataEvent() and tests; keep stable.
std::ostream &operator<<(std::ostream &os, const MarketDataEvent &e);

} // namespace cmf
