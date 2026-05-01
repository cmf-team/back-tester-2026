#pragma once
#include "DataReader.hpp"
#include "Metrics.hpp"
#include "OrderManager.hpp"

// Базовый интерфейс стратегии
class IStrategy
{
  public:
    virtual ~IStrategy() = default;

    // Вызывается на каждом снапшоте стакана
    // Стратегия решает куда ставить bid/ask
    virtual void on_book(const BookSnapshot& snap,
                         OrderManager& om,
                         Metrics& metrics) = 0;

    // Вызывается на каждой рыночной сделке
    // Проверяем исполнились ли наши ордера
    virtual void on_trade(const Trade& trade,
                          OrderManager& om,
                          Metrics& metrics) = 0;

    virtual const char* name() const = 0;
};
