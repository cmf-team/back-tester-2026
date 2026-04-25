#include "dispatcher/ShardedDispatcher.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <utility>

namespace cmf
{

ShardedDispatcher::ShardedDispatcher(std::size_t num_workers,
                                     std::size_t queue_capacity)
    : num_workers_(num_workers)
{
    if (num_workers_ == 0 || num_workers_ > kMaxWorkers)
        throw std::invalid_argument(
            "ShardedDispatcher: num_workers must be in [1, kMaxWorkers]");

    workers_.reserve(num_workers_);
    for (std::size_t i = 0; i < num_workers_; ++i)
    {
        auto w = std::make_unique<Worker>();
        w->queue = std::make_unique<SpscRingQueue<MarketDataEvent>>(queue_capacity);
        workers_.push_back(std::move(w));
    }
    for (auto& w : workers_)
    {
        w->thread = std::thread([raw = w.get()]
                                { runWorker(*raw); });
    }
}

ShardedDispatcher::~ShardedDispatcher()
{
    try
    {
        finalize();
    }
    catch (...)
    {
        // best-effort shutdown — never throw from destructor
    }
}

std::size_t ShardedDispatcher::shardOf(std::uint64_t iid) const noexcept
{
    // Mix bits a bit so even/odd / nearby ids don't all land on shard 0.
    // A 64-bit splitmix step is overkill; cheap multiplicative hash suffices.
    std::uint64_t h = iid * 11400714819323198485ULL;
    return static_cast<std::size_t>(h >> 56) % num_workers_;
}

void ShardedDispatcher::dispatch(const MarketDataEvent& event)
{
    ++stats_.events_total;

    std::uint64_t iid = event.instrument_id;
    if (iid == 0 && event.action != MdAction::Add &&
        event.action != MdAction::Clear)
    {
        const auto it = order_to_iid_.find(event.order_id);
        if (it != order_to_iid_.end())
        {
            iid = it->second;
        }
        else
        {
            ++stats_.unresolved_iid;
            return;
        }
    }

    if (event.action == MdAction::Add && event.instrument_id != 0)
        order_to_iid_[event.order_id] = iid;

    // Patch iid into the event so the worker can route locally.
    MarketDataEvent patched = event;
    patched.instrument_id = iid;

    ++stats_.events_routed;
    const std::size_t shard = shardOf(iid);
    auto& q = *workers_[shard]->queue;
    while (!q.push(patched))
    {
        std::this_thread::yield();
    }
}

void ShardedDispatcher::runWorker(Worker& w)
{
    MarketDataEvent ev;
    auto& books = w.books;
    auto& orders = w.orders;

    while (!w.queue->done())
    {
        if (!w.queue->pop(ev))
        {
            std::this_thread::yield();
            continue;
        }
        ++w.events;
        const std::uint64_t iid = ev.instrument_id;

        switch (ev.action)
        {
        case MdAction::Add:
        {
            if (ev.side == Side::None || !ev.priceDefined() || ev.size == 0)
                break;
            books[iid].applyDelta(ev.side, ev.price,
                                  static_cast<std::int64_t>(ev.size));
            OrderState st;
            st.instrument_id = iid;
            st.side = ev.side;
            st.price = ev.price;
            st.qty = static_cast<std::int64_t>(ev.size);
            orders[ev.order_id] = st;
            break;
        }
        case MdAction::Cancel:
        case MdAction::Trade:
        case MdAction::Fill:
        {
            const auto it = orders.find(ev.order_id);
            if (it == orders.end())
                break;
            OrderState& st = it->second;
            LimitOrderBook& book = books[st.instrument_id];
            const std::int64_t reduce_qty = std::min<std::int64_t>(
                st.qty, static_cast<std::int64_t>(ev.size));
            if (reduce_qty <= 0)
                break;
            book.applyDelta(st.side, st.price, -reduce_qty);
            st.qty -= reduce_qty;
            if (st.qty <= 0)
                orders.erase(it);
            break;
        }
        case MdAction::Modify:
        {
            const auto it = orders.find(ev.order_id);
            if (it == orders.end())
                break;
            OrderState& st = it->second;
            LimitOrderBook& book = books[st.instrument_id];
            book.applyDelta(st.side, st.price, -st.qty);
            if (ev.priceDefined())
                st.price = ev.price;
            if (ev.size > 0)
                st.qty = static_cast<std::int64_t>(ev.size);
            if (ev.side != Side::None)
                st.side = ev.side;
            if (st.qty > 0)
                book.applyDelta(st.side, st.price, st.qty);
            else
                orders.erase(it);
            break;
        }
        case MdAction::Clear:
        {
            auto it = books.find(iid);
            if (it != books.end())
                it->second.clear();
            for (auto oit = orders.begin(); oit != orders.end();)
            {
                if (oit->second.instrument_id == iid)
                    oit = orders.erase(oit);
                else
                    ++oit;
            }
            break;
        }
        case MdAction::None:
        default:
            break;
        }
    }
}

ShardedDispatcherStats ShardedDispatcher::finalize()
{
    if (!finalized_)
    {
        finalized_ = true;
        for (auto& w : workers_)
        {
            w->queue->close();
        }
        for (auto& w : workers_)
        {
            if (w->thread.joinable())
                w->thread.join();
        }
        stats_.per_worker_events.clear();
        stats_.per_worker_instruments.clear();
        stats_.per_worker_orders_active.clear();
        stats_.per_worker_events.reserve(workers_.size());
        stats_.per_worker_instruments.reserve(workers_.size());
        stats_.per_worker_orders_active.reserve(workers_.size());
        for (const auto& w : workers_)
        {
            stats_.per_worker_events.push_back(w->events);
            stats_.per_worker_instruments.push_back(w->books.size());
            stats_.per_worker_orders_active.push_back(w->orders.size());
        }
    }
    return stats_;
}

} // namespace cmf
