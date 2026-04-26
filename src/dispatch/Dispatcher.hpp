#pragma once

#include "common/MarketDataEvent.hpp"
#include "dispatch/EventSink.hpp"
#include "dispatch/LobRegistry.hpp"
#include "lob/LimitOrderBook.hpp"
#include "lob/OrderIndex.hpp"

#include <cstddef>

namespace cmf {

template <class Merger>
class Dispatcher {
public:
    Dispatcher(Merger& merger, LobRegistry& registry, OrderIndex& index, EventSink& sink) noexcept
        : merger_(merger), registry_(registry), index_(index), sink_(sink) {}

    void run() {
        MarketDataEvent e;
        while (merger_.next(e)) {
            dispatch(e);
            ++count_;
        }
    }

    std::size_t events_processed() const noexcept { return count_; }
    std::size_t trades_seen()      const noexcept { return trade_count_; }
    std::size_t orphans_skipped()  const noexcept { return orphans_; }
    NanoTime    first_ts()         const noexcept { return first_ts_; }
    NanoTime    last_ts()          const noexcept { return last_ts_; }

    void dispatch(const MarketDataEvent& e) {
        if (count_ == 0) first_ts_ = e.ts_recv;
        last_ts_ = e.ts_recv;

        switch (e.action) {
        case 'A': handle_add(e);    break;
        case 'C': handle_cancel(e); break;
        case 'M': handle_modify(e); break;
        case 'F': handle_fill(e);   break;
        case 'T': handle_trade(e);  break;
        case 'R': handle_reset(e);  break;
        default:                    break;
        }
    }

private:
    void handle_add(const MarketDataEvent& e) {
        const uint32_t inst = e.instrument_id;
        if (inst == 0) { ++orphans_; return; }

        const auto px = LimitOrderBook::scale(e.price);
        OrderRecord rec{inst, e.side, px, e.size};
        if (e.order_id != 0) index_.insert(e.order_id, rec);

        auto& book = registry_.get_or_create(inst);
        book.apply_add(e.side, px, e.size);
        sink_.on_event(e, book);
    }

    void handle_cancel(const MarketDataEvent& e) {
        OrderRecord rec;
        if (!resolve(e, rec)) return;
        auto& book = registry_.get_or_create(rec.instrument_id);
        const uint64_t cancel_qty = e.size != 0 ? e.size : rec.remaining_qty;
        book.apply_cancel(rec.side, rec.scaled_price, cancel_qty);
        if (cancel_qty >= rec.remaining_qty) index_.erase(e.order_id);
        else index_.update_qty(e.order_id, rec.remaining_qty - cancel_qty);
        sink_.on_event(e, book);
    }

    void handle_modify(const MarketDataEvent& e) {
        OrderRecord rec;
        if (!resolve(e, rec)) return;
        auto& book = registry_.get_or_create(rec.instrument_id);
        book.apply_cancel(rec.side, rec.scaled_price, rec.remaining_qty);

        const auto new_px  = LimitOrderBook::scale(e.price);
        const char new_side = e.side != 'N' ? e.side : rec.side;
        OrderRecord new_rec{rec.instrument_id, new_side, new_px, e.size};
        book.apply_add(new_side, new_px, e.size);
        if (e.order_id != 0) index_.insert(e.order_id, new_rec);
        sink_.on_event(e, book);
    }

    void handle_fill(const MarketDataEvent& e) {
        OrderRecord rec;
        if (!resolve(e, rec)) return;
        auto& book = registry_.get_or_create(rec.instrument_id);
        const uint64_t fill_qty = e.size;
        book.apply_fill(rec.side, rec.scaled_price, fill_qty);
        if (fill_qty >= rec.remaining_qty) index_.erase(e.order_id);
        else index_.update_qty(e.order_id, rec.remaining_qty - fill_qty);
        sink_.on_event(e, book);
    }

    void handle_trade(const MarketDataEvent& e) {
        ++trade_count_;
        sink_.on_trade(e);
    }

    void handle_reset(const MarketDataEvent& e) {
        const uint32_t inst = e.instrument_id;
        if (inst == 0) return;
        registry_.get_or_create(inst).clear();
        sink_.on_clear(inst);
    }

    bool resolve(const MarketDataEvent& e, OrderRecord& out) {
        if (e.order_id != 0 && index_.find(e.order_id, out)) return true;
        if (e.instrument_id != 0) {
            out = OrderRecord{e.instrument_id, e.side, LimitOrderBook::scale(e.price), e.size};
            return true;
        }
        ++orphans_;
        return false;
    }

    Merger&      merger_;
    LobRegistry& registry_;
    OrderIndex&  index_;
    EventSink&   sink_;
    std::size_t  count_       = 0;
    std::size_t  trade_count_ = 0;
    std::size_t  orphans_     = 0;
    NanoTime     first_ts_    = 0;
    NanoTime     last_ts_     = 0;
};

} // namespace cmf
