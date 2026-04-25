#pragma once
#include "MarketDataEvent.hpp"
#include <map>
#include <unordered_map>
#include <functional>
#include <ostream>

// Книга заявок (LOB) для одного инструмента.
// Поддерживает операции Add/Cancel/Modify/Clear в O(log N).
// Trade и Fill не изменяют книгу напрямую — их обрабатывает
// LOB для статистики, но не для удаления ордеров
// (Cancel/Fill придут отдельными сообщениями от биржи).
class LimitOrderBook {
public:
    // Применить одно MBO-событие к стакану.
    // Диспетчеризует по event.action на соответствующий private-метод.
    void applyEvent(const MarketDataEvent& event);

    // Вернуть лучшую цену бида (std::nullopt если бидов нет)
    std::optional<double> bestBid() const;
    // Вернуть лучшую цену аска (std::nullopt если асков нет)
    std::optional<double> bestAsk() const;

    // Суммарный объём на лучшем биде / аске
    long long bestBidSize() const;
    long long bestAskSize() const;

    // Вывести snapshot стакана (top N уровней с каждой стороны)
    void printSnapshot(std::ostream& os, int depth = 5) const;

    // Счётчики событий для статистики
    long long totalAdds    = 0;
    long long totalCancels = 0;
    long long totalTrades  = 0;
    long long totalClears  = 0;

private:
    // action='A': вставить новую заявку в стакан.
    // Если order_id уже существует — обновить size (idempotent replay).
    void onAdd(const MarketDataEvent& e);

    // action='C': удалить заявку по order_id.
    // Используем orderIndex для O(1) нахождения уровня цены.
    void onCancel(const MarketDataEvent& e);

    // action='M': изменить цену/объём заявки.
    // = onCancel(старый) + onAdd(новый).
    void onModify(const MarketDataEvent& e);

    // action='R': полный сброс стакана (Clear).
    // Биржа присылает перед snapshot-восстановлением.
    void onClear();

    // action='T': сделка (агрессор). Не изменяет стакан.
    void onTrade(const MarketDataEvent& e);

    // action='F': исполнение пассивного ордера. Уменьшает size.
    void onFill(const MarketDataEvent& e);

    // Биды: цена -> (order_id -> size). std::greater<> => begin() = лучший бид.
    std::map<double, std::map<std::string, long long>, std::greater<double>> bids_;
    // Аски: цена -> (order_id -> size). begin() = лучший аск (наименьшая цена).
    std::map<double, std::map<std::string, long long>> asks_;
    // Индекс: order_id -> {side, price} для O(1) Cancel/Fill
    std::unordered_map<std::string, std::pair<char, double>> orderIndex_;
};
