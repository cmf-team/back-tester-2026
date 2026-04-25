#include "LimitOrderBook.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>

void LimitOrderBook::applyEvent(const MarketDataEvent& event) {
    // Диспетчер: маршрутизировать событие на метод по полю action.
    // Порядок case выбран по частоте в XEUR.EOBI: A и C доминируют (~50/50).
    switch (event.action) {
        case 'A': onAdd(event);    break;
        case 'C': onCancel(event); break;
        case 'M': onModify(event); break;
        case 'R': onClear();       break;
        case 'T': onTrade(event);  break;
        case 'F': onFill(event);   break;
        default: break; // action='N' — ничего не делать
    }
}

void LimitOrderBook::onAdd(const MarketDataEvent& e) {
    // Бизнес-логика: новый лимитный ордер выставлен на биржу.
    // Нужно добавить его в нужную сторону стакана по цене.
    if (!e.price.has_value() || e.size <= 0) return; // защита от некорректных данных
    const double px = e.priceAsDouble();
    // Записать order_id в индекс для быстрого Cancel
    orderIndex_[e.orderId] = {e.side, px};
    if (e.side == 'B') {
        bids_[px][e.orderId] = e.size;
    } else if (e.side == 'A') {
        asks_[px][e.orderId] = e.size;
    }
    ++totalAdds;
}

void LimitOrderBook::onCancel(const MarketDataEvent& e) {
    // Бизнес-логика: участник отозвал ордер с рынка.
    // Найти ордер через индекс и удалить из соответствующего ценового уровня.
    auto it = orderIndex_.find(e.orderId);
    if (it == orderIndex_.end()) return; // ордер не найден — пропустить
    const auto [side, px] = it->second;
    orderIndex_.erase(it);
    if (side == 'B') {
        auto lvl = bids_.find(px);
        if (lvl != bids_.end()) {
            lvl->second.erase(e.orderId);
            if (lvl->second.empty()) bids_.erase(lvl); // удалить пустой уровень
        }
    } else {
        auto lvl = asks_.find(px);
        if (lvl != asks_.end()) {
            lvl->second.erase(e.orderId);
            if (lvl->second.empty()) asks_.erase(lvl);
        }
    }
    ++totalCancels;
}

void LimitOrderBook::printSnapshot(std::ostream& os, int depth) const {
    os << "  --- ASK ---\n";
    // Итерировать по аскам в обратном порядке чтобы показать сверху (дальние)
    int cnt = 0;
    std::vector<decltype(asks_)::const_iterator> askLevels;
    for (auto it = asks_.begin(); it != asks_.end() && cnt < depth; ++it, ++cnt)
        askLevels.push_back(it);
    for (auto it = askLevels.rbegin(); it != askLevels.rend(); ++it) {
        long long vol = 0;
        for (auto& [oid, sz] : (*it)->second) vol += sz;
        os << std::fixed << std::setprecision(6)
           << "  " << (*it)->first << "  x" << vol << "\n";
    }
    os << "  -----------\n";
    cnt = 0;
    for (auto it = bids_.begin(); it != bids_.end() && cnt < depth; ++it, ++cnt) {
        long long vol = 0;
        for (auto& [oid, sz] : it->second) vol += sz;
        os << std::fixed << std::setprecision(6)
           << "  " << it->first << "  x" << vol << "\n";
    }
    os << "  --- BID ---\n";
}


void LimitOrderBook::onModify(const MarketDataEvent& e)
{
    // BUSINESS:
    // Modify means the existing resting order changed price and/or size.
    // For LOB reconstruction this is equivalent to:
    // 1) remove old order contribution;
    // 2) insert the new version.
    onCancel(e);
    onAdd(e);
}

void LimitOrderBook::onClear()
{
    // BUSINESS:
    // Clear means the exchange says: current book state is no longer valid.
    // We must delete all remembered orders and all aggregated price levels.
    bids_.clear();
    asks_.clear();
    orderIndex_.clear();
    ++totalClears;
}

void LimitOrderBook::onTrade(const MarketDataEvent& e)
{
    // BUSINESS:
    // Databento docs define Trade as aggressor-side trade event.
    // It does not directly change the resting book.
    // Passive liquidity reduction is represented by Fill/Cancel records.
    (void)e;
    ++totalTrades;
}

void LimitOrderBook::onFill(const MarketDataEvent& e)
{
    // BUSINESS:
    // Fill means a resting order was executed.
    // In this simplified LOB engine we remove the order like Cancel.
    // If partial fills are present, this can be extended to reduce by e.size.
    onCancel(e);
}

std::optional<double> LimitOrderBook::bestBid() const
{
    if (bids_.empty())
        return std::nullopt;

    return bids_.begin()->first;
}

std::optional<double> LimitOrderBook::bestAsk() const
{
    if (asks_.empty())
        return std::nullopt;

    return asks_.begin()->first;
}

long long LimitOrderBook::bestBidSize() const
{
    if (bids_.empty())
        return 0;

    long long total = 0;
    for (const auto& [orderId, size] : bids_.begin()->second)
        total += size;

    return total;
}

long long LimitOrderBook::bestAskSize() const
{
    if (asks_.empty())
        return 0;

    long long total = 0;
    for (const auto& [orderId, size] : asks_.begin()->second)
        total += size;

    return total;
}