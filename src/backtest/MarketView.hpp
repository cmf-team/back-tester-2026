#pragma once

#include "book/BookManager.hpp"
#include "book/LimitOrderBook.hpp"

#include <cstdint>
#include <optional>

namespace md {

struct MarketView {
    std::uint64_t instrument_id{};
    std::uint64_t timestamp{};

    std::optional<std::int64_t> best_bid;
    std::optional<std::int64_t> best_ask;

    std::uint64_t best_bid_size{};
    std::uint64_t best_ask_size{};

    std::optional<std::int64_t> mid_price;
    std::optional<std::int64_t> microprice;
    std::optional<std::int64_t> spread;
};

[[nodiscard]] MarketView makeMarketView(const LimitOrderBook& book, std::uint64_t timestamp);
[[nodiscard]] MarketView makeMarketView(
    const BookManager& book_manager,
    std::uint64_t instrument_id,
    std::uint64_t timestamp
);

} // namespace md
