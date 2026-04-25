#include "LimitOrderBook.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

// ── L2 helpers ────────────────────────────────────────────────────────────────

void LimitOrderBook::addToLevel(Side side, int64_t price, uint32_t qty)
{
    if (qty == 0)
        return;
    if (side == Side::Bid)
        bids_[price] += qty;
    else if (side == Side::Ask)
        asks_[price] += qty;
}

void LimitOrderBook::subtractFromLevel(Side side, int64_t price, uint32_t qty)
{
    if (qty == 0)
        return;
    if (side == Side::Bid)
    {
        auto it = bids_.find(price);
        if (it == bids_.end())
            return;
        if (it->second <= qty)
            bids_.erase(it);
        else
            it->second -= qty;
    }
    else if (side == Side::Ask)
    {
        auto it = asks_.find(price);
        if (it == asks_.end())
            return;
        if (it->second <= qty)
            asks_.erase(it);
        else
            it->second -= qty;
    }
}

// ── Event dispatch ─────────────────────────────────────────────────────────────

void LimitOrderBook::applyEvent(const MarketDataEvent& event)
{
    switch (event.action)
    {
    case Action::Add:
    {
        if (event.side == Side::None || event.size == 0)
            break;
        orders_[event.order_id] = {event.side, event.price, event.size};
        addToLevel(event.side, event.price, event.size);
        break;
    }

    case Action::Cancel:
    {
        // event.size = remaining qty after cancel (0 = fully cancelled)
        auto it = orders_.find(event.order_id);
        if (it == orders_.end())
            break;
        auto& e = it->second;
        uint32_t rem = event.size;
        if (rem >= e.size)
        {
            subtractFromLevel(e.side, e.price, e.size);
            orders_.erase(it);
        }
        else
        {
            subtractFromLevel(e.side, e.price, e.size - rem);
            e.size = rem;
        }
        break;
    }

    case Action::Modify:
    {
        // event carries new price and new size
        auto it = orders_.find(event.order_id);
        if (it == orders_.end())
            break;
        auto& e = it->second;
        subtractFromLevel(e.side, e.price, e.size);
        e.price = event.price;
        e.size = event.size;
        addToLevel(e.side, e.price, e.size);
        break;
    }

    case Action::Fill:
    {
        // Resting order was partially or fully filled; event.size = remaining qty
        auto it = orders_.find(event.order_id);
        if (it == orders_.end())
            break;
        auto& e = it->second;
        uint32_t rem = event.size;
        if (rem >= e.size)
        {
            subtractFromLevel(e.side, e.price, e.size);
            orders_.erase(it);
        }
        else
        {
            subtractFromLevel(e.side, e.price, e.size - rem);
            e.size = rem;
        }
        break;
    }

    case Action::Trade:
        // Aggressor-side trade — informational, resting book unchanged here.
        break;

    case Action::Clear:
        orders_.clear();
        bids_.clear();
        asks_.clear();
        break;

    case Action::None:
        break;
    }
}

// ── Queries ────────────────────────────────────────────────────────────────────

std::optional<std::pair<int64_t, uint32_t>> LimitOrderBook::bestBid() const
{
    if (bids_.empty())
        return std::nullopt;
    auto it = bids_.begin();
    return std::make_pair(it->first, it->second);
}

std::optional<std::pair<int64_t, uint32_t>> LimitOrderBook::bestAsk() const
{
    if (asks_.empty())
        return std::nullopt;
    auto it = asks_.begin();
    return std::make_pair(it->first, it->second);
}

uint32_t LimitOrderBook::volumeAtPrice(int64_t price) const
{
    if (auto it = bids_.find(price); it != bids_.end())
        return it->second;
    if (auto it = asks_.find(price); it != asks_.end())
        return it->second;
    return 0;
}

// ── Snapshot ──────────────────────────────────────────────────────────────────

void LimitOrderBook::printSnapshot(uint32_t instrument_id, int depth) const
{
    std::cout << std::fixed << std::setprecision(9);
    std::cout << "\n=== LOB [instrument_id=" << instrument_id << "] ===\n";

    // Collect the first `depth` ask levels (ascending) and display highest first.
    std::vector<std::pair<int64_t, uint32_t>> ask_levels;
    for (auto it = asks_.begin(); it != asks_.end() && (int)ask_levels.size() < depth; ++it)
        ask_levels.emplace_back(it->first, it->second);

    for (auto it = ask_levels.rbegin(); it != ask_levels.rend(); ++it)
    {
        std::cout << "  ASK  " << std::setw(16) << MarketDataEvent::priceToDouble(it->first)
                  << "  x  " << it->second << "\n";
    }

    auto bb = bestBid();
    auto ba = bestAsk();
    if (bb && ba)
    {
        double spread = MarketDataEvent::priceToDouble(ba->first - bb->first);
        std::cout << "  --- spread: " << spread << " ---\n";
    }
    else
    {
        std::cout << "  --- spread: N/A ---\n";
    }

    int count = 0;
    for (auto it = bids_.begin(); it != bids_.end() && count < depth; ++it, ++count)
    {
        std::cout << "  BID  " << std::setw(16) << MarketDataEvent::priceToDouble(it->first)
                  << "  x  " << it->second << "\n";
    }

    std::cout << "  [orders=" << orders_.size()
              << "  bid_levels=" << bids_.size()
              << "  ask_levels=" << asks_.size() << "]\n";
}
