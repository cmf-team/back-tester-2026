#pragma once

#include "domain/MarketDataEvent.hpp"

namespace md {

struct QueueItem {
    bool end_of_stream{false};
    MarketDataEvent event{};

    static QueueItem data(const MarketDataEvent& event) {
        return QueueItem{false, event};
    }

    static QueueItem end() {
        return QueueItem{true, MarketDataEvent{}};
    }
};

} // namespace md
