
//



#pragma once

#include "common/MarketDataEvent.hpp"

#include <cstdint>
#include <iosfwd>
#include <map>
#include <unordered_map>

namespace cmf {

class LimitOrderBook {
public:


  bool apply(const MarketDataEvent &ev);


  std::int64_t bestBid() const;
  std::int64_t bestAsk() const;


  std::int64_t volumeAtPrice(SideChar side, std::int64_t price) const;


  std::size_t bidLevels() const { return bids_.size(); }
  std::size_t askLevels() const { return asks_.size(); }


  std::size_t orderCount() const { return orders_.size(); }


  void printSnapshot(std::ostream &os, std::size_t depth = 10) const;


  void clear();

private:


  struct OrderInfo {
    std::int64_t price;
    std::int64_t size;
    SideChar     side;
  };


  std::unordered_map<std::uint64_t, OrderInfo> orders_;




  std::map<std::int64_t, std::int64_t, std::greater<>> bids_;
  std::map<std::int64_t, std::int64_t>                 asks_;


  void addToLevel(SideChar side, std::int64_t price, std::int64_t qty);
  void removeFromLevel(SideChar side, std::int64_t price, std::int64_t qty);
};

} // namespace cmf
