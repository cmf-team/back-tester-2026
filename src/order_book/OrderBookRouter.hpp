#pragma once

#include "common/MarketDataEvent.hpp"
#include <ostream>
#include <string>

namespace cmf {
template <typename Derived> class OrderBookRouter {
public:
  void apply(const MarketDataEvent &e) {
    static_cast<Derived *>(this)->apply_impl(e);
  }

  void print_snapshot(std::ostream &out, const size_t group_by_levels) const {
    static_cast<const Derived *>(this)->print_snapshot_impl(out,
                                                            group_by_levels);
  }

  void print_best_bid_ask(std::ostream &out) const {
    static_cast<const Derived *>(this)->print_best_bid_ask_impl(out);
  }

  [[nodiscard]] std::string
  snapshot_as_string(const std::size_t group_by_levels) const {
    return static_cast<const Derived *>(this)->snapshot_as_string_impl(
        group_by_levels);
  }
};

} // namespace cmf
