// Holds one of several concrete source types by value. Exposes the duck-typed
// next() contract and dispatches to the held alternative with a compile-time-
// unrolled if-chain on the variant's tag — this compiles to a jump table /
// direct calls, unlike std::visit which goes through a static fn-ptr table.

#pragma once

#include "parser/MarketDataEvent.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace cmf {

template <class... Ts>
class VariantSource {
public:
  template <class T,
            class = std::enable_if_t<
                (std::is_same_v<std::decay_t<T>, Ts> || ...)>>
  VariantSource(T&& t) : v_(std::forward<T>(t)) {}

  bool next(MarketDataEvent& out) { return nextImpl<0>(out); }

private:
  template <std::size_t I>
  bool nextImpl(MarketDataEvent& out) {
    if constexpr (I < sizeof...(Ts)) {
      if (v_.index() == I) return std::get<I>(v_).next(out);
      return nextImpl<I + 1>(out);
    } else {
      __builtin_unreachable();
    }
  }

  std::variant<Ts...> v_;
};

} // namespace cmf
