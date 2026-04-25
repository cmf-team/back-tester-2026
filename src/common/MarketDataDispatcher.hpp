//


#pragma once

#include "common/LimitOrderBook.hpp"
#include "common/MarketDataEvent.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace cmf {
    class MarketDataDispatcher {
    public:
        bool dispatch(const MarketDataEvent &ev);


        LimitOrderBook &book(std::uint32_t instrument_id);


        const LimitOrderBook *find(std::uint32_t instrument_id) const;


        std::size_t instrumentCount() const { return books_.size(); }


        void rememberSymbol(std::uint32_t instrument_id, const std::string &symbol);

        const std::string &symbolOf(std::uint32_t instrument_id) const;


        void printSnapshot(std::ostream &os, std::uint32_t instrument_id,
                           std::size_t depth = 10) const;


        void printAllBestQuotes(std::ostream &os) const;

    private:
        std::unordered_map<std::uint32_t, LimitOrderBook> books_;
        std::unordered_map<std::uint32_t, std::string> symbols_;


        std::unordered_map<std::uint64_t, std::uint32_t> order_to_instr_;
    };
} // namespace cmf
