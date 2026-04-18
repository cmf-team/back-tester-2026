#pragma once

#include "common/MarketDataEvent.hpp"
#include <concepts>
#include <filesystem>
#include <utility>

namespace cmf {
template <typename Derived> class DataParser {
protected:
  std::filesystem::path path_;

public:
  explicit DataParser(std::filesystem::path path) : path_(std::move(path)) {}

  template <std::invocable<const MarketDataEvent &> F> void parse(F &&f) const {
    static_cast<const Derived *>(this)->parse_inner(std::forward<F>(f));
  }
};
} // namespace cmf
