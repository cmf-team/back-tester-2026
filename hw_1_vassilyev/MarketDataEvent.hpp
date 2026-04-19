#pragma once

#include <cstdint>
#include <string>

struct MarketDataEvent {
    std::string timestamp;     // hd.ts_event
    std::string order_id;
    char side;                 // 'B'=Bid, 'A'=Ask, 'N'=None
    double price;              // 0.0 if null
    uint32_t size;
    char action;               // 'R'=Reset, 'A'=Add, 'C'=Cancel, 'M'=Modify, 'T'=Trade, 'F'=Fill
    uint32_t instrument_id;
    std::string symbol;
};
