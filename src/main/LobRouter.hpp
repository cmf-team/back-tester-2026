#pragma once
#include "LimitOrderBook.hpp"
#include "MarketDataEvent.hpp"
#include <unordered_map>
#include <string>

// Маршрутизатор событий -> LOB.
// Ведёт отдельный LOB для каждого instrument_id.
// Решает, когда печатать промежуточные snapshot-ы.
class LobRouter {
public:
    // Сконструировать роутер.
    // snapshotIntervalEvents: печатать snapshot каждые N событий (0 = не печатать).
    explicit LobRouter(std::size_t snapshotIntervalEvents = 50000);

    // Принять событие и направить в соответствующий LOB.
    // Вызывается последовательно для каждого события из хронологически
    // упорядоченного потока (producer).
    void route(const MarketDataEvent& event);

    // Вывести финальное состояние: best bid/ask для каждого инструмента.
    void printFinalState(std::ostream& os) const;

    // Вывести агрегированную статистику по всем инструментам.
    void printStats(std::ostream& os) const;

private:
    // instrument_id -> его LOB
    std::unordered_map<long long, LimitOrderBook> lobs_;
    // instrument_id -> символ (для читаемого вывода)
    std::unordered_map<long long, std::string>    symbols_;
    std::size_t totalEventsRouted_ = 0;
    std::size_t snapshotInterval_  = 0;
    // Отслеживаем момент последнего snapshot для F_LAST-события
    std::size_t lastSnapshotAt_ = 0;
};
