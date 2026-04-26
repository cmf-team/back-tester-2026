#pragma once

#include "common/MarketDataEvent.hpp"
#include "common/SpscQueue.hpp"
#include "dispatch/EventSink.hpp"
#include "dispatch/LobRegistry.hpp"
#include "lob/LimitOrderBook.hpp"
#include "lob/OrderIndex.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace cmf {

struct RoutedEvent {
    NanoTime    ts            = 0;
    uint64_t    order_id      = 0;
    int64_t     scaled_price  = 0;
    uint64_t    in_qty        = 0;
    uint32_t    instrument_id = 0;
    uint32_t    remaining_qty = 0;
    uint8_t     op            = 0;
    char        side          = 'N';

    static constexpr NanoTime SENTINEL = MarketDataEvent::SENTINEL;
    enum : uint8_t { OP_ADD=1, OP_CANCEL=2, OP_MODIFY_OLD=3, OP_MODIFY_NEW=4, OP_FILL=5, OP_RESET=6 };
};

using RoutingQueue = SpscQueue<RoutedEvent, 16384>;

class LobWorker {
public:
    LobWorker(RoutingQueue& in, LobRegistry& books) noexcept
        : in_(in), books_(books) {}

    void start() { thread_ = std::thread(&LobWorker::run, this); }
    void join()  { if (thread_.joinable()) thread_.join(); }

    std::size_t events_applied() const noexcept { return applied_; }

private:
    void run() {
        RoutedEvent r;
        while (true) {
            in_.pop(r);
            if (r.ts == RoutedEvent::SENTINEL) break;
            apply(r);
            ++applied_;
        }
    }

    void apply(const RoutedEvent& r) {
        auto& book = books_.get_or_create(r.instrument_id);
        switch (r.op) {
        case RoutedEvent::OP_ADD:        book.apply_add   (r.side, r.scaled_price, r.in_qty); break;
        case RoutedEvent::OP_CANCEL:     book.apply_cancel(r.side, r.scaled_price, r.in_qty); break;
        case RoutedEvent::OP_MODIFY_OLD: book.apply_cancel(r.side, r.scaled_price, r.in_qty); break;
        case RoutedEvent::OP_MODIFY_NEW: book.apply_add   (r.side, r.scaled_price, r.in_qty); break;
        case RoutedEvent::OP_FILL:       book.apply_fill  (r.side, r.scaled_price, r.in_qty); break;
        case RoutedEvent::OP_RESET:      book.clear(); break;
        }
    }

    RoutingQueue& in_;
    LobRegistry&  books_;
    std::thread   thread_;
    std::size_t   applied_ = 0;
};

template <class Merger>
class ShardedDispatcher {
public:
    ShardedDispatcher(Merger& merger, OrderIndex& index, std::size_t shards)
        : merger_(merger), index_(index), n_(shards) {
        queues_.reserve(n_);
        registries_.reserve(n_);
        workers_.reserve(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            queues_.emplace_back(std::make_unique<RoutingQueue>());
            registries_.emplace_back(std::make_unique<LobRegistry>());
            workers_.emplace_back(std::make_unique<LobWorker>(*queues_.back(), *registries_.back()));
        }
    }

    void run() {
        for (auto& w : workers_) w->start();

        MarketDataEvent e;
        while (merger_.next(e)) {
            route(e);
            ++count_;
            if (first_ts_ == 0) first_ts_ = e.ts_recv;
            last_ts_ = e.ts_recv;
        }

        RoutedEvent done{};
        done.ts = RoutedEvent::SENTINEL;
        for (auto& q : queues_) q->push(done);
        for (auto& w : workers_) w->join();
    }

    std::size_t           events_processed() const noexcept { return count_; }
    std::size_t           orphans_skipped()  const noexcept { return orphans_; }
    std::size_t           trades_seen()      const noexcept { return trades_; }
    NanoTime              first_ts()         const noexcept { return first_ts_; }
    NanoTime              last_ts()          const noexcept { return last_ts_; }
    std::size_t           shards()           const noexcept { return n_; }
    const LobRegistry&    registry(std::size_t i) const { return *registries_[i]; }

    std::size_t total_books() const {
        std::size_t s = 0;
        for (auto& r : registries_) s += r->size();
        return s;
    }

private:
    static uint32_t shard_of(uint32_t inst, std::size_t n) noexcept {
        return static_cast<uint32_t>((inst * 2654435761u) % n);
    }

    void route(const MarketDataEvent& e) {
        switch (e.action) {
        case 'A': route_add(e);     break;
        case 'C': route_cancel(e);  break;
        case 'M': route_modify(e);  break;
        case 'F': route_fill(e);    break;
        case 'T': ++trades_;        break;
        case 'R': route_reset(e);   break;
        default: break;
        }
    }

    void route_add(const MarketDataEvent& e) {
        if (e.instrument_id == 0) { ++orphans_; return; }
        const auto px = LimitOrderBook::scale(e.price);
        if (e.order_id != 0)
            index_.insert(e.order_id, {e.instrument_id, e.side, px, e.size});

        RoutedEvent r{};
        r.ts            = e.ts_recv;
        r.order_id      = e.order_id;
        r.scaled_price  = px;
        r.in_qty        = e.size;
        r.instrument_id = e.instrument_id;
        r.op            = RoutedEvent::OP_ADD;
        r.side          = e.side;
        queues_[shard_of(e.instrument_id, n_)]->push(r);
    }

    void route_cancel(const MarketDataEvent& e) {
        OrderRecord rec;
        if (!resolve(e, rec)) return;
        const uint64_t qty = e.size != 0 ? e.size : rec.remaining_qty;

        RoutedEvent r{};
        r.ts            = e.ts_recv;
        r.order_id      = e.order_id;
        r.scaled_price  = rec.scaled_price;
        r.in_qty        = qty;
        r.instrument_id = rec.instrument_id;
        r.op            = RoutedEvent::OP_CANCEL;
        r.side          = rec.side;
        queues_[shard_of(rec.instrument_id, n_)]->push(r);

        if (qty >= rec.remaining_qty) index_.erase(e.order_id);
        else                          index_.update_qty(e.order_id, rec.remaining_qty - qty);
    }

    void route_modify(const MarketDataEvent& e) {
        OrderRecord rec;
        if (!resolve(e, rec)) return;

        const auto new_px   = LimitOrderBook::scale(e.price);
        const char new_side = e.side != 'N' ? e.side : rec.side;
        const auto shard    = shard_of(rec.instrument_id, n_);

        RoutedEvent old_evt{};
        old_evt.ts            = e.ts_recv;
        old_evt.order_id      = e.order_id;
        old_evt.scaled_price  = rec.scaled_price;
        old_evt.in_qty        = rec.remaining_qty;
        old_evt.instrument_id = rec.instrument_id;
        old_evt.op            = RoutedEvent::OP_MODIFY_OLD;
        old_evt.side          = rec.side;
        queues_[shard]->push(old_evt);

        RoutedEvent new_evt{};
        new_evt.ts            = e.ts_recv;
        new_evt.order_id      = e.order_id;
        new_evt.scaled_price  = new_px;
        new_evt.in_qty        = e.size;
        new_evt.instrument_id = rec.instrument_id;
        new_evt.op            = RoutedEvent::OP_MODIFY_NEW;
        new_evt.side          = new_side;
        queues_[shard]->push(new_evt);

        if (e.order_id != 0)
            index_.insert(e.order_id, {rec.instrument_id, new_side, new_px, e.size});
    }

    void route_fill(const MarketDataEvent& e) {
        OrderRecord rec;
        if (!resolve(e, rec)) return;
        const uint64_t qty = e.size;

        RoutedEvent r{};
        r.ts            = e.ts_recv;
        r.order_id      = e.order_id;
        r.scaled_price  = rec.scaled_price;
        r.in_qty        = qty;
        r.instrument_id = rec.instrument_id;
        r.op            = RoutedEvent::OP_FILL;
        r.side          = rec.side;
        queues_[shard_of(rec.instrument_id, n_)]->push(r);

        if (qty >= rec.remaining_qty) index_.erase(e.order_id);
        else                          index_.update_qty(e.order_id, rec.remaining_qty - qty);
    }

    void route_reset(const MarketDataEvent& e) {
        if (e.instrument_id == 0) return;
        RoutedEvent r{};
        r.ts            = e.ts_recv;
        r.instrument_id = e.instrument_id;
        r.op            = RoutedEvent::OP_RESET;
        queues_[shard_of(e.instrument_id, n_)]->push(r);
    }

    bool resolve(const MarketDataEvent& e, OrderRecord& out) {
        if (e.order_id != 0 && index_.find(e.order_id, out)) return true;
        if (e.instrument_id != 0) {
            out = {e.instrument_id, e.side, LimitOrderBook::scale(e.price), e.size};
            return true;
        }
        ++orphans_;
        return false;
    }

    Merger&     merger_;
    OrderIndex& index_;
    std::size_t n_;
    std::vector<std::unique_ptr<RoutingQueue>> queues_;
    std::vector<std::unique_ptr<LobRegistry>>  registries_;
    std::vector<std::unique_ptr<LobWorker>>    workers_;
    std::size_t count_    = 0;
    std::size_t orphans_  = 0;
    std::size_t trades_   = 0;
    NanoTime    first_ts_ = 0;
    NanoTime    last_ts_  = 0;
};

} // namespace cmf
