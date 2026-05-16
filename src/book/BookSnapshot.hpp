#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace md {

struct PriceLevelSnapshot {
    std::int64_t price{};
    std::uint64_t size{};
};

struct InstrumentBookSnapshot {
    std::uint64_t instrument_id{};
    std::size_t resting_orders{};
    std::optional<std::int64_t> best_bid;
    std::optional<std::int64_t> best_ask;
    std::vector<PriceLevelSnapshot> bids;
    std::vector<PriceLevelSnapshot> asks;
};

struct BookManagerSnapshot {
    std::size_t event_count{};
    std::uint64_t timestamp{};
    std::size_t processed_events{};
    std::size_t unresolved_events{};
    std::vector<InstrumentBookSnapshot> instruments;
};

} // namespace md
