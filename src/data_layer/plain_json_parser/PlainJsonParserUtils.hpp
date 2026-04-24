#pragma once

#include <charconv>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>

namespace data_layer::json_line {

void skipWhitespace(const char *&p, const char *const end);

bool isBlankLine(const std::string_view line) noexcept;

bool skipToKey(const char *&p, const char *const end,
                 const std::string_view key) noexcept;

bool readQuotedView(const char *&p, const char *const end,
                      std::string_view &out) noexcept;

template <typename IntT>
inline bool readIntegral(const char *&p, const char *const end,
                          IntT &out) noexcept {
  using ParseT = std::conditional_t<
      std::is_signed_v<IntT>,
      std::conditional_t<(sizeof(IntT) < sizeof(std::int64_t)), std::int64_t,
                         IntT>,
      std::conditional_t<(sizeof(IntT) < sizeof(std::uint64_t)), std::uint64_t,
                         IntT>>;

  skipWhitespace(p, end);
  ParseT tmp{};
  const auto [ptr, ec] = std::from_chars(p, end, tmp);
  if (ec != std::errc{} || ptr == p) {
    return false;
  }
  if (tmp < static_cast<ParseT>(std::numeric_limits<IntT>::min()) ||
      tmp > static_cast<ParseT>(std::numeric_limits<IntT>::max())) {
    return false;
  }
  out = static_cast<IntT>(tmp);
  p = ptr;
  return true;
}

std::optional<std::uint64_t>
parseUtcTimestampNs(const std::string_view timestamp) noexcept;

std::optional<std::int64_t>
parsePrice1e9(const std::string_view price_text) noexcept;

} 
