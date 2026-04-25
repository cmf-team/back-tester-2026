#include "LobRouter.hpp"
#include <iostream>

LobRouter::LobRouter(std::size_t interval) : snapshotInterval_(interval) {}

void LobRouter::route(const MarketDataEvent& event) {
    // Запомнить символ при первом появлении инструмента
    if (!event.symbol.empty())
        symbols_.emplace(event.instrumentId, event.symbol);

    // Найти или создать LOB для данного instrument_id.
    // operator[] создаёт LOB по умолчанию если не существует.
    LimitOrderBook& lob = lobs_[event.instrumentId];
    lob.applyEvent(event);
    ++totalEventsRouted_;

    // Печатать промежуточный snapshot если:
    // - задан интервал,
    // - пришло время по счётчику,
    // - и текущее событие — последнее в атомарной группе (F_LAST),
    //   чтобы не показывать стакан в промежуточном состоянии.
    if (snapshotInterval_ > 0
        && totalEventsRouted_ - lastSnapshotAt_ >= snapshotInterval_
        && event.isLastInEvent()) {
        lastSnapshotAt_ = totalEventsRouted_;
        std::cout << "\n[SNAPSHOT @ event " << totalEventsRouted_
                  << " ts=" << event.tsEvent << "]\n";
        std::cout << "Instrument: " << event.symbol
                  << " (id=" << event.instrumentId << ")\n";
        lob.printSnapshot(std::cout, 5);
    }
}


void LobRouter::printFinalState(std::ostream& os) const
{
    os << "\n===== FINAL BEST BID / ASK =====\n";

    for (const auto& [instrumentId, lob] : lobs_)
    {
        auto bid = lob.bestBid();
        auto ask = lob.bestAsk();

        os << "instrument_id=" << instrumentId;

        auto symIt = symbols_.find(instrumentId);
        if (symIt != symbols_.end())
            os << " symbol=\"" << symIt->second << "\"";

        os << " best_bid=" << (bid ? std::to_string(*bid) : "null")
            << " best_bid_size=" << lob.bestBidSize()
            << " best_ask=" << (ask ? std::to_string(*ask) : "null")
            << " best_ask_size=" << lob.bestAskSize()
            << "\n";
    }
}

void LobRouter::printStats(std::ostream& os) const
{
    os << "\n===== ROUTER STATS =====\n";
    os << "Events routed: " << totalEventsRouted_ << "\n";
    os << "Instruments:   " << lobs_.size() << "\n";
}