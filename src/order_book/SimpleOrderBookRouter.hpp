#pragma once

#include "MapOrderBook.hpp"
#include "OrderBookRouter.hpp"
#include <memory_resource>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace cmf {
template <typename BookType = MapOrderBook>
class SimpleOrderBookRouter
    : public OrderBookRouter<SimpleOrderBookRouter<BookType>> {
  friend class OrderBookRouter<SimpleOrderBookRouter<BookType>>;

private:
  std::pmr::memory_resource *mr_;
  std::pmr::unordered_map<uint32_t, BookType> order_books_;
  std::pmr::unordered_map<uint64_t, uint32_t> order_to_instrument_;

public:
  explicit SimpleOrderBookRouter(
      std::pmr::memory_resource *mr = std::pmr::get_default_resource())
      : mr_(mr), order_books_{mr_}, order_to_instrument_{mr_} {}

  void apply_impl(const MarketDataEvent &e) {
    if (e.instrument_id != 0 && e.order_id != 0)
      order_to_instrument_[e.order_id] = e.instrument_id;

    uint32_t instr_id = order_to_instrument_[e.order_id];

    if (instr_id == 0 && e.order_id != 0) [[unlikely]] {
      throw std::runtime_error(
          "SimpleOrderBookRouter: cannot resolve instrument_id for order_id " +
          std::to_string(e.order_id));
    }

    order_books_[instr_id].apply(e);
  }

  void print_snapshot_impl(std::ostream &out,
                           const size_t group_by_levels) const {
    for (auto [instrument_id, order_book] : order_books_) {
      const auto best_bid = order_book.best_price(Side::Buy);
      const auto best_ask = order_book.best_price(Side::Sell);

      if (!best_bid && !best_ask)
        continue;

      out << "// " << instrument_id << "\n";
      order_book.print_snapshot(out, group_by_levels);
    }
  }

  void print_best_bid_ask_impl(std::ostream &out) const {
    out << "\n// ====== Final Best Bid/Ask ======\n";
    for (const auto &[instrument_id, order_book] : order_books_) {
      auto best_bid = order_book.best_price(Side::Buy);
      auto best_ask = order_book.best_price(Side::Sell);

      if (!best_bid && !best_ask)
        continue;

      out << "Instrument " << instrument_id << ":\n";

      if (best_bid) {
        auto bid_volume = order_book.volume_at(Side::Buy, *best_bid);
        out << "  Best Bid: " << *best_bid << " x " << bid_volume << "\n";
      } else {
        out << "  Best Bid: (empty)\n";
      }

      if (best_ask) {
        auto ask_volume = order_book.volume_at(Side::Sell, *best_ask);
        out << "  Best Ask: " << *best_ask << " x " << ask_volume << "\n";
      } else {
        out << "  Best Ask: (empty)\n";
      }
    }
    out << "// ====== End Best Bid/Ask ======\n";
  }

  [[nodiscard]] std::string
  snapshot_as_string_impl(const std::size_t group_by_levels) const {
    std::ostringstream oss;
    for (auto &[instrument_id, order_book] : order_books_) {
      const auto best_bid = order_book.best_price(Side::Buy);
      const auto best_ask = order_book.best_price(Side::Sell);

      if (!best_bid && !best_ask)
        continue;

      oss << "// " << instrument_id << "\n";
      order_book.print_snapshot(oss, group_by_levels);
    }
    return std::move(oss).str();
  }
};

} // namespace cmf
