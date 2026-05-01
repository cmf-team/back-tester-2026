#pragma once
#include <cstdint>
#include <optional>

// Активный ордер стратегии (не путать с Order из src/common/Order.hpp)
struct StrategyOrder
{
    uint64_t id;
    bool is_buy;
    double price;
    double amount;
    bool active = true;
};

class OrderManager
{
  public:
    uint64_t next_id = 1;

    std::optional<StrategyOrder> bid_order;
    std::optional<StrategyOrder> ask_order;

    void place(double bid_price, double ask_price, double amount)
    {
        bid_order = StrategyOrder{next_id++, true, bid_price, amount, true};
        ask_order = StrategyOrder{next_id++, false, ask_price, amount, true};
    }

    void cancel_all()
    {
        bid_order.reset();
        ask_order.reset();
    }

    bool check_bid_fill(double trade_price, bool is_sell) const
    {
        return bid_order.has_value() && bid_order->active && is_sell && trade_price <= bid_order->price;
    }

    bool check_ask_fill(double trade_price, bool is_sell) const
    {
        return ask_order.has_value() && ask_order->active && !is_sell && trade_price >= ask_order->price;
    }

    void fill_bid()
    {
        if (bid_order)
            bid_order->active = false;
    }
    void fill_ask()
    {
        if (ask_order)
            ask_order->active = false;
    }

    double partial_fill_bid(double trade_amount)
    {
        if (!bid_order || !bid_order->active)
            return 0.0;
        double filled = std::min(trade_amount, bid_order->amount);
        bid_order->amount -= filled;
        if (bid_order->amount <= 0)
            bid_order->active = false;
        return filled;
    }

    double partial_fill_ask(double trade_amount)
    {
        if (!ask_order || !ask_order->active)
            return 0.0;
        double filled = std::min(trade_amount, ask_order->amount);
        ask_order->amount -= filled;
        if (ask_order->amount <= 0)
            ask_order->active = false;
        return filled;
    }
};
