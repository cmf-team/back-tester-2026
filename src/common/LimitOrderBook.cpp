#include "LimitOrderBook.hpp"
#include <algorithm>

void LimitOrderBook::Add(uint64_t order_id, int64_t price, int64_t quantity,
                         cmf::Side side) {
  if (side == cmf::Side::Buy)
    ask[price] += quantity;
  else
    bid[price] += quantity;

  orderbook[order_id].first = price;
  orderbook[order_id].second = quantity;
}

void LimitOrderBook::Cancel(uint64_t order_id, int64_t price, int64_t quantity,
                            cmf::Side side) {
  if (side == cmf::Side::Buy)
    ask[price] -= quantity;
  else
    bid[price] -= quantity;

  orderbook.erase(order_id);
}

void LimitOrderBook::Modify(uint64_t order_id, int64_t price, int64_t quantity,
                            cmf::Side side) {
  auto &old = orderbook[order_id];

  if (side == cmf::Side::Buy)
    ask[old.first] -= old.second;
  else
    bid[old.first] -= old.second;

  old.first = price;
  old.second = quantity;

  if (side == cmf::Side::Buy)
    ask[price] += quantity;
  else
    bid[price] += quantity;
}

void LimitOrderBook::Trade(int64_t, int64_t, cmf::Side) {
  // Does not affect the book
}

void LimitOrderBook::Fill(int64_t, int64_t, cmf::Side) {
  // Does not affect the book
}

int64_t LimitOrderBook::best_bid() {
  if (bid.empty())
    return 0;
  return std::max_element(
             bid.begin(), bid.end(),
             [](const auto &a, const auto &b) { return a.first < b.first; })
      ->first;
}

int64_t LimitOrderBook::best_ask() {
  if (ask.empty())
    return 0;
  return std::min_element(
             ask.begin(), ask.end(),
             [](const auto &a, const auto &b) { return a.first < b.first; })
      ->first;
}

int64_t LimitOrderBook::volume_at_price(int64_t price, cmf::Side side) {
  const auto &book = (side == cmf::Side::Buy) ? ask : bid;
  auto it = book.find(price);
  return it != book.end() ? it->second : 0;
}
