#pragma once

#include "common/BasicTypes.hpp"

#include <cstdint>
#include <string>

namespace cmf {
    enum class Action : char {
        None = 'N',
        Add = 'A',
        Modify = 'M',
        Cancel = 'C',
        Clear = 'R',
        Trade = 'T',
        Fill = 'F',
    };


    enum class SideChar : char {
        None = 'N',
        Buy = 'B',
        Ask = 'A',
    };

    class MarketDataEvent {
    public:
        NanoTime ts_recv = 0;
        NanoTime ts_event = 0;

        std::uint16_t publisher_id = 0;
        std::uint32_t instrument_id = 0;

        Action action = Action::None;
        SideChar side = SideChar::None;


        static constexpr std::int64_t UNDEF_PRICE = 9223372036854775807LL; // INT64_MAX
        std::int64_t price = UNDEF_PRICE;

        std::int64_t size = 0;


        std::uint64_t order_id = 0;

        std::uint8_t flags = 0;
        std::int32_t ts_in_delta = 0;
        std::uint32_t sequence = 0;


        std::string symbol;
    };
} // namespace cmf
