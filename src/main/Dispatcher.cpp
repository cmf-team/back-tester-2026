#include "Dispatcher.hpp"

namespace cmf {

void Dispatcher::process(const MarketDataEvent& event) {
    ++eventCount_;

    switch (event.action) {
        case Action::Add: {
            // remember this order so future Cancel/Modify/Fill can find it
            orders_[event.order_id] = {
                event.instrument_id, event.side, event.price, event.size
            };
            books_[event.instrument_id].addLiquidity(
                event.side, event.price, event.size
            );
            ++eventsPerInstrument_[event.instrument_id];
            break;
        }

        case Action::Cancel:
        case Action::Fill: {
            // event may not carry instrument_id / side / price — look them up
            auto it = orders_.find(event.order_id);
            if (it == orders_.end()) break;
            ResidentOrder& ord = it->second;

            books_[ord.instrument_id].removeLiquidity(
                ord.side, ord.price, event.size
            );
            ord.size -= event.size;
            ++eventsPerInstrument_[ord.instrument_id];

            if (ord.size <= 0.0) {
                orders_.erase(it);
            }
            break;
        }

        case Action::Modify: {
            auto it = orders_.find(event.order_id);
            if (it == orders_.end()) {
                // unseen order — treat as Add
                orders_[event.order_id] = {
                    event.instrument_id, event.side, event.price, event.size
                };
                books_[event.instrument_id].addLiquidity(
                    event.side, event.price, event.size
                );
                ++eventsPerInstrument_[event.instrument_id];
                break;
            }
            ResidentOrder& ord = it->second;

            // remove old level, add new level
            books_[ord.instrument_id].removeLiquidity(ord.side, ord.price, ord.size);
            books_[ord.instrument_id].addLiquidity(event.side, event.price, event.size);

            ord.side  = event.side;
            ord.price = event.price;
            ord.size  = event.size;
            ++eventsPerInstrument_[ord.instrument_id];
            break;
        }

        case Action::Clear: {
            auto it = books_.find(event.instrument_id);
            if (it != books_.end()) {
                it->second.clearBook();
            }
            ++eventsPerInstrument_[event.instrument_id];
            break;
        }

        case Action::Trade:
        case Action::None:
        default:
            // informational: no book mutation
            break;
    }
}

size_t Dispatcher::bookCount() const         { return books_.size(); }
size_t Dispatcher::orderCount() const        { return orders_.size(); }
size_t Dispatcher::eventsProcessed() const   { return eventCount_; }

const LimitOrderBook* Dispatcher::book(uint32_t instrument_id) const {
    auto it = books_.find(instrument_id);
    return (it == books_.end()) ? nullptr : &it->second;
}

uint32_t Dispatcher::mostActiveInstrument() const {
    uint32_t best = 0;
    size_t   bestCount = 0;
    for (const auto& [id, count] : eventsPerInstrument_) {
        if (count > bestCount) {
            bestCount = count;
            best = id;
        }
    }
    return best;
}

} // namespace cmf
