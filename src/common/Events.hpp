#pragma once

#include <cstdint>
#include <string>

namespace domain::events {


inline constexpr std::int64_t UNDEF_PRICE{9223372036854775807LL};


struct MdHeader {
  
  std::uint64_t ts_event;
  
  std::uint8_t rtype;
  
  std::uint16_t publisher_id;
  
  std::uint32_t instrument_id;
};

struct MarketDataEvent {
  
  std::string ts_recv;
  MdHeader hd;
  
  char action;
  
  char side;
  
  std::int64_t price;
  
  std::uint32_t size;
  
  std::uint8_t channel_id;
  
  std::uint64_t order_id;
  
  std::uint8_t flags;
  
  std::int32_t ts_in_delta;
  
  std::uint32_t sequence;
  
  std::string symbol;
};

const MarketDataEvent EOF_EVENT{
    .ts_recv = {},
    .hd = {},
    .action = {},
    .side = {},
    .price = {},
    .size = {},
    .channel_id = {},
    .order_id = {},
    .flags = {},
    .ts_in_delta = {},
    .sequence = {},
    .symbol = "EOF",
};

} 
