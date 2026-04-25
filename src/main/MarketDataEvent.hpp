#pragma once

#include <optional>
#include <string>
#include <ostream>

#include "json.hpp"

using json = nlohmann::json;


// Одно сообщение из Databento MBO JSONL-файла.
// Соответствует схеме rtype=0xA0 (MBO record) документации Databento.
// Поля хранятся как строки там, где биржа/Databento передаёт ISO8601 или decimal строки.
struct MarketDataEvent {
    // Временны́е метки (ISO 8601 при pretty_ts=true)
    std::string tsRecv;   // время приёма Databento (GPS, монотонно)
    std::string tsEvent;  // время биржи (tag 60 FIX, наносекунды)

    static MarketDataEvent fromJson(const json& j);

    // Заголовок (hd)
    int rtype        = 0;   // тип записи: 160=MBO, 32-35=OHLCV, ...
    int publisherId  = 0;   // ID публикатора (101 = Eurex EOBI)
    long long instrumentId = 0; // ID конкретного инструмента (опциона/фьючерса)

    // Поля ордера
    char action = 'N'; // A=Add C=Cancel M=Modify T=Trade F=Fill R=Clear N=None
    char side   = 'N'; // B=Buy(bid) A=Ask/sell N=нет стороны
    std::optional<std::string> price; // decimal строка, null если цена не задана
    long long size      = 0;
    long long channelId = 0;
    std::string orderId;  // uint64 передаётся как строка во избежание потери точности
    int flags            = 0;  // битовое поле: 128=F_LAST, 64=F_TOB, ...
    long long tsInDelta  = 0;  // нс между ts_recv и ts_publisher_send
    unsigned long long sequence = 0;
    std::string symbol;  // человекочитаемый тикер (map_symbols=true)

    std::string priceOrNull() const {
        return price.has_value() ? *price : "null";
    }
    double priceAsDouble() const {
        // Преобразовать decimal строку в double для LOB.
        // Вызывать только если price.has_value().
        return price.has_value() ? std::stod(*price) : 0.0;
    }
    bool isLastInEvent() const {
        // F_LAST (бит 7 = 128): после этого события стакан консистентен,
        // можно безопасно снимать snapshot.
        return (flags & 128) != 0;
    }
};

std::ostream& operator<<(std::ostream& os, const MarketDataEvent& e);
