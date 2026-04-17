#pragma once

#include <cstdint>
#include <limits>
#include <string>

class MarketDataEvent {
public:
  enum class Side : std::uint8_t { Bid, Ask, None };

  enum class Action : std::uint8_t {
    Add,
    Modify,
    Cancel,
    Clear,
    Trade,
    Fill,
    None
  };

  using price_t = std::int64_t;
  static constexpr std::uint64_t UNDEF_TIMESTAMP =
      std::numeric_limits<std::uint64_t>::max();
  static constexpr std::int64_t UNDEF_PRICE =
      std::numeric_limits<std::int64_t>::max();

  MarketDataEvent() = default;

  MarketDataEvent(std::uint64_t sort_ts_, std::uint64_t ts_recv_,
                  std::uint64_t ts_event_, std::int32_t ts_in_delta_,
                  std::uint32_t publisher_id_, std::uint32_t instrument_id_,
                  std::uint64_t order_id_, price_t price_, std::uint32_t size_,
                  Side side_, Action action_, std::uint8_t flags_,
                  std::uint8_t rtype_, std::uint32_t source_file_id_)
      : sort_ts(sort_ts_), ts_recv(ts_recv_), ts_event(ts_event_),
        ts_in_delta(ts_in_delta_), publisher_id(publisher_id_),
        instrument_id(instrument_id_), order_id(order_id_), price(price_),
        size(size_), side(side_), action(action_), flags(flags_), rtype(rtype_),
        source_file_id(source_file_id_) {}

  [[nodiscard]] std::uint64_t getSortTs() const noexcept { return sort_ts; }
  [[nodiscard]] std::uint64_t getTsRecv() const noexcept { return ts_recv; }
  [[nodiscard]] std::uint64_t getTsEvent() const noexcept { return ts_event; }
  [[nodiscard]] std::int32_t getTsInDelta() const noexcept {
    return ts_in_delta;
  }
  [[nodiscard]] std::uint32_t getPublisherId() const noexcept {
    return publisher_id;
  }
  [[nodiscard]] std::uint32_t getInstrumentId() const noexcept {
    return instrument_id;
  }
  [[nodiscard]] std::uint64_t getOrderId() const noexcept { return order_id; }
  [[nodiscard]] price_t getPrice() const noexcept { return price; }
  [[nodiscard]] std::uint32_t getSize() const noexcept { return size; }
  [[nodiscard]] Side getSide() const noexcept { return side; }
  [[nodiscard]] Action getAction() const noexcept { return action; }
  [[nodiscard]] std::uint8_t getFlags() const noexcept { return flags; }
  [[nodiscard]] std::uint8_t getRType() const noexcept { return rtype; }
  [[nodiscard]] std::uint32_t getSourceFileId() const noexcept {
    return source_file_id;
  }

  [[nodiscard]] static const char *sideToString(Side s) noexcept;
  [[nodiscard]] static const char *actionToString(Action a) noexcept;

private:
  std::uint64_t sort_ts{UNDEF_TIMESTAMP};
  std::uint64_t ts_recv{UNDEF_TIMESTAMP};
  std::uint64_t ts_event{UNDEF_TIMESTAMP};
  std::int32_t ts_in_delta{0};

  std::uint32_t publisher_id{0};
  std::uint32_t instrument_id{0};
  std::uint64_t order_id{0};

  price_t price{UNDEF_PRICE};
  std::uint32_t size{0};

  Side side{Side::None};
  Action action{Action::None};

  std::uint8_t flags{0};
  std::uint8_t rtype{0};
  std::uint32_t source_file_id{0};
};