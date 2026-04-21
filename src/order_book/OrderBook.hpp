#pragma once

#include <map>
#include <ostream>
#include <vector>

#include "common/MarketDataEvent.hpp"

namespace cmf {
template <typename Derived> class OrderBook {
public:
  void apply(const MarketDataEvent &event) {
    static_cast<Derived *>(this)->apply_impl(event);
  }

  [[nodiscard]] std::optional<int64_t> best_price(Side side) const noexcept {
    return static_cast<const Derived *>(this)->best_price_impl(side);
  }

  [[nodiscard]] uint64_t volume_at(Side side, int64_t price) const noexcept {
    return static_cast<const Derived *>(this)->volume_at_impl(side, price);
  }

  [[nodiscard]] bool empty(Side side) const noexcept {
    return static_cast<const Derived *>(this)->empty_impl(side);
  }

  [[nodiscard]] decltype(auto) side_levels(Side side) const {
    return static_cast<const Derived *>(this)->side_levels_impl(side);
  }

  void print_snapshot(std::ostream &out) const;

  void print_snapshot(std::ostream &out, size_t group_by_levels) const;
};

template <typename Derived>
void OrderBook<Derived>::print_snapshot(std::ostream &out) const {
  for (const auto side : {Side::Buy, Side::Sell}) {
    out << "// ----------------\n";
    out << "// " << (side == Side::Buy ? "BUY" : "SELL") << "\n";
    out << "// ----------------\n";
    for (const auto &[price, qty] : this->side_levels(side))
      out << "  " << price << " : " << qty << "\n";
  }
}

template <typename Derived>
void OrderBook<Derived>::print_snapshot(std::ostream &out,
                                        const size_t group_by_levels) const {
  for (const auto side : {Side::Buy, Side::Sell}) {
    out << "// " << (side == Side::Buy ? "BUY" : "SELL") << "\n";

    auto levels = this->side_levels(side);
    if (levels.empty()) {
      out << "  (empty)\n";
      continue;
    }

    std::vector<std::pair<int64_t, uint64_t>> level_list(levels.begin(),
                                                         levels.end());
    size_t levels_per_group =
        (level_list.size() + group_by_levels - 1) / group_by_levels;

    for (size_t g = 0;
         g < group_by_levels && g * levels_per_group < level_list.size(); ++g) {
      size_t start = g * levels_per_group;
      size_t end = std::min((g + 1) * levels_per_group, level_list.size());

      int64_t min_price = level_list[start].first;
      int64_t max_price = level_list[end - 1].first;
      uint64_t total_qty = 0;

      for (size_t i = start; i < end; ++i)
        total_qty += level_list[i].second;

      out << "  [" << min_price << ", " << max_price << "] : " << total_qty
          << "\n";
    }
  }
}
} // namespace cmf
