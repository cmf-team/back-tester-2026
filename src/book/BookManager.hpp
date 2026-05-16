#pragma once

#include "book/BookSnapshot.hpp"
#include "book/LimitOrderBook.hpp"
#include "domain/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace md {

class BookManager {
public:
    void apply(const MarketDataEvent& event);

    [[nodiscard]] const LimitOrderBook* findBook(std::uint64_t instrument_id) const;
    [[nodiscard]] LimitOrderBook& getOrCreateBook(std::uint64_t instrument_id);

    [[nodiscard]] std::size_t instrumentCount() const noexcept;
    [[nodiscard]] std::size_t processedEvents() const noexcept;
    [[nodiscard]] std::size_t unresolvedEvents() const noexcept;
    [[nodiscard]] std::string stableStateDigest() const;
    [[nodiscard]] BookManagerSnapshot snapshot(
        std::size_t event_count,
        std::uint64_t timestamp,
        std::size_t depth
    ) const;

    void printSnapshot(std::ostream& out, std::size_t depth) const;
    void printFinalBestBidAsk(std::ostream& out) const;

private:
    [[nodiscard]] std::uint64_t resolveInstrumentId(const MarketDataEvent& event) const;
    void updateOrderMapping(const MarketDataEvent& event, const LimitOrderBook& book);
    void eraseOrderMappingIfMatches(std::uint64_t order_id, std::uint64_t instrument_id);
    void eraseOrderMappingsForInstrument(std::uint64_t instrument_id);
    void removePreviousInstrumentMapping(const MarketDataEvent& event, std::uint64_t target_instrument_id);

    std::unordered_map<std::uint64_t, LimitOrderBook> books_by_instrument_;
    std::unordered_map<std::uint64_t, std::uint64_t> order_to_instrument_;
    std::size_t processed_events_{};
    std::size_t unresolved_events_{};
};

} // namespace md
