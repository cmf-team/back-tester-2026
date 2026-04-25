#pragma once
#include "common/MarketDataEvent.hpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <unordered_map>

namespace cmf
{

class LimitOrderBook
{
  private:
    struct OrderInfo
    {
        Price price;
        Quantity size;
        Side side;
    };

    std::unordered_map<OrderId, OrderInfo> orders_;
    std::map<Price, Quantity, std::greater<Price>> bids_;
    std::map<Price, Quantity, std::less<Price>> asks_;

    bool isZero(Quantity q) const { return q <= 1e-9; }

  public:
    void apply(const MarketDataEvent& ev)
    {
        switch (ev.action)
        {
        case Action::Clear:
            orders_.clear();
            bids_.clear();
            asks_.clear();
            break;

        case Action::Add:
        {
            if (ev.price <= 1e-9)
                break;
            orders_[ev.order_id] = {ev.price, ev.size, ev.side};
            if (ev.side == Side::Buy)
                bids_[ev.price] += ev.size;
            else if (ev.side == Side::Sell)
                asks_[ev.price] += ev.size;
            break;
        }

        case Action::Cancel:
        {
            auto it = orders_.find(ev.order_id);
            if (it != orders_.end())
            {
                if (it->second.side == Side::Buy)
                {
                    bids_[it->second.price] -= ev.size;
                    if (isZero(bids_[it->second.price]))
                        bids_.erase(it->second.price);
                }
                else if (it->second.side == Side::Sell)
                {
                    asks_[it->second.price] -= ev.size;
                    if (isZero(asks_[it->second.price]))
                        asks_.erase(it->second.price);
                }
                it->second.size -= ev.size;
                if (isZero(it->second.size))
                    orders_.erase(it);
            }
            break;
        }

        case Action::Modify:
        {
            if (ev.price <= 1e-9)
                break;
            auto it = orders_.find(ev.order_id);
            if (it != orders_.end())
            {
                if (it->second.side == Side::Buy)
                {
                    bids_[it->second.price] -= it->second.size;
                    if (isZero(bids_[it->second.price]))
                        bids_.erase(it->second.price);
                }
                else if (it->second.side == Side::Sell)
                {
                    asks_[it->second.price] -= it->second.size;
                    if (isZero(asks_[it->second.price]))
                        asks_.erase(it->second.price);
                }

                it->second.price = ev.price;
                it->second.size = ev.size;

                if (it->second.side == Side::Buy)
                    bids_[ev.price] += ev.size;
                else if (it->second.side == Side::Sell)
                    asks_[ev.price] += ev.size;
            }
            break;
        }

        case Action::Trade:
        case Action::Fill:
        case Action::None:
            break;
        }
    }

    void printBest() const
    {
        std::cout << "BBO -> ";
        if (!bids_.empty())
        {
            std::cout << "Bid: " << bids_.begin()->second << " @ "
                      << std::fixed << std::setprecision(5) << bids_.begin()->first;
        }
        else
        {
            std::cout << "Bid: NONE";
        }

        std::cout << " | ";

        if (!asks_.empty())
        {
            std::cout << "Ask: " << asks_.begin()->second << " @ "
                      << std::fixed << std::setprecision(5) << asks_.begin()->first;
        }
        else
        {
            std::cout << "Ask: NONE";
        }
        std::cout << "\n";
    }

    void printSnapshot(uint32_t instr_id, NanoTime ts) const
    {
        std::cout << "\n=== LOB Snapshot | Instr: " << instr_id << " | TS: " << ts << " ===\n";

        auto ask_it = asks_.rbegin();
        int count = 0;
        while (ask_it != asks_.rend() && count < 5)
        {
            std::cout << "ASK  | Px: " << std::fixed << std::setprecision(5) << ask_it->first
                      << " | Vol: " << ask_it->second << "\n";
            ++ask_it;
            ++count;
        }

        std::cout << "--------------------------------------\n";

        auto bid_it = bids_.begin();
        count = 0;
        while (bid_it != bids_.end() && count < 5)
        {
            std::cout << "BID  | Px: " << std::fixed << std::setprecision(5) << bid_it->first
                      << " | Vol: " << bid_it->second << "\n";
            ++bid_it;
            ++count;
        }
        std::cout << "======================================\n";
    }
};

} // namespace cmf