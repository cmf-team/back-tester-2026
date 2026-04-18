#pragma once

#include <string>
#include <cstdint>
#include "BasicTypes.hpp"

namespace cmf {

struct MarketDataEvent {
    std::string tsRecvStr;
    std::string tsEventStr;

    NanoTime tsRecv = 0;
    NanoTime tsEvent = 0;

    std::uint8_t  rtype = 0;
    std::uint16_t publisherId = 0;
    std::uint32_t instrumentId = 0;

    char action = 'N';
    char side   = 'N';

    std::string priceStr;
    std::uint32_t size = 0;
    std::string orderIdStr;

    std::uint8_t  flags = 0;
    std::int32_t  tsInDelta = 0;
    std::uint64_t sequence  = 0;

    std::string symbol;

    std::string sortKeyStr() const {
        return !tsRecvStr.empty() ? tsRecvStr : tsEventStr;
    }
};

}