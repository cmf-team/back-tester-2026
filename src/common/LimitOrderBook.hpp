#pragma once

#include "BasicTypes.hpp"
#include <unordered_map>

struct LimitOrderBook {
  std::unordered_map<int64_t, int64_t> bid, ask;
  std::unordered_map<uint64_t, std::pair<int64_t, int64_t>> orderbook;

  void Add(uint64_t order_id, int64_t price, int64_t quantity, cmf::Side side);
  void Cancel(uint64_t order_id, int64_t price, int64_t quantity,
              cmf::Side side);
  void Modify(uint64_t order_id, int64_t price, int64_t quantity,
              cmf::Side side);
  void Trade(int64_t price, int64_t quantity, cmf::Side side);
  void Fill(int64_t price, int64_t quantity, cmf::Side side);

  int64_t best_bid();
  int64_t best_ask();
  int64_t volume_at_price(int64_t price, cmf::Side side);
};
