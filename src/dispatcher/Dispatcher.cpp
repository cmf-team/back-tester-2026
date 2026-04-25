#include "dispatcher/Dispatcher.hpp"

#include "common/BasicTypes.hpp"
#include "market_data/MarketDataEvent.hpp"
#include "order_book/LimitOrderBook.hpp"

#include <algorithm>
#include <cstdint>
#include <ios>
#include <ostream>
#include <utility>
#include <vector>

namespace cmf
{

namespace
{

double scaledToDouble(std::int64_t scaled)
{
    return static_cast<double>(scaled) /
           static_cast<double>(MarketDataEvent::kPriceScale);
}

} // namespace

Dispatcher::Dispatcher() : Dispatcher(Options{}) {}

Dispatcher::Dispatcher(Options opts)
    : opts_(opts), snapshot_worker_(opts.snapshot_out) {}

void Dispatcher::dispatch(const MarketDataEvent& event)
{
    ++stats_.events_total;

    // Resolve instrument_id. Cancel/Modify/Trade/Fill messages may reference
    // an order that was Add'd earlier, possibly with a different iid carrier.
    std::uint64_t iid = event.instrument_id;
    if (iid == 0 && event.action != MdAction::Add &&
        event.action != MdAction::Clear)
    {
        const auto it = orders_.find(event.order_id);
        if (it != orders_.end())
        {
            iid = it->second.instrument_id;
        }
        else
        {
            ++stats_.unresolved_iid;
            return; // nothing we can do — drop quietly
        }
    }

    switch (event.action)
    {
    case MdAction::Add:
    {
        if (event.side == Side::None || !event.priceDefined() || event.size == 0)
            break;
        LimitOrderBook& book = books_[iid];
        book.applyDelta(event.side, event.price,
                        static_cast<std::int64_t>(event.size));
        OrderState st;
        st.instrument_id = iid;
        st.side = event.side;
        st.price = event.price;
        st.qty = static_cast<std::int64_t>(event.size);
        orders_[event.order_id] = st;
        ++stats_.events_routed;
        break;
    }
    case MdAction::Cancel:
    case MdAction::Trade:
    case MdAction::Fill:
    {
        const auto it = orders_.find(event.order_id);
        if (it == orders_.end())
            break;
        OrderState& st = it->second;
        LimitOrderBook& book = books_[st.instrument_id];
        const std::int64_t reduce_qty =
            std::min<std::int64_t>(st.qty, static_cast<std::int64_t>(event.size));
        if (reduce_qty <= 0)
            break;
        book.applyDelta(st.side, st.price, -reduce_qty);
        st.qty -= reduce_qty;
        if (st.qty <= 0)
            orders_.erase(it);
        ++stats_.events_routed;
        break;
    }
    case MdAction::Modify:
    {
        const auto it = orders_.find(event.order_id);
        if (it == orders_.end())
            break;
        OrderState& st = it->second;
        LimitOrderBook& book = books_[st.instrument_id];
        book.applyDelta(st.side, st.price, -st.qty);
        if (event.priceDefined())
            st.price = event.price;
        if (event.size > 0)
            st.qty = static_cast<std::int64_t>(event.size);
        if (event.side != Side::None)
            st.side = event.side;
        if (st.qty > 0)
            book.applyDelta(st.side, st.price, st.qty);
        else
            orders_.erase(it);
        ++stats_.events_routed;
        break;
    }
    case MdAction::Clear:
    {
        auto it = books_.find(iid);
        if (it != books_.end())
            it->second.clear();
        // Drop all orders that referenced this instrument.
        for (auto oit = orders_.begin(); oit != orders_.end();)
        {
            if (oit->second.instrument_id == iid)
                oit = orders_.erase(oit);
            else
                ++oit;
        }
        ++stats_.events_routed;
        break;
    }
    case MdAction::None:
    default:
        break;
    }

    maybeEmitSnapshot();
}

void Dispatcher::maybeEmitSnapshot()
{
    if (opts_.snapshot_every == 0 || opts_.snapshot_out == nullptr)
        return;
    if (stats_.events_total % opts_.snapshot_every != 0)
        return;

    SnapshotTask task;
    task.event_index = stats_.events_total;
    task.entries.reserve(
        std::min(books_.size(), opts_.snapshot_max_instruments));
    std::size_t emitted = 0;
    for (const auto& [iid, book] : books_)
    {
        if (emitted >= opts_.snapshot_max_instruments)
            break;
        SnapshotEntry e;
        e.instrument_id = iid;
        e.bbo = book.bbo();
        task.entries.push_back(std::move(e));
        ++emitted;
    }
    snapshot_worker_.enqueue(std::move(task));
}

DispatcherStats Dispatcher::finalize()
{
    if (!finalized_)
    {
        finalized_ = true;
        snapshot_worker_.stop();
        stats_.orders_active = orders_.size();
        stats_.instruments_touched = books_.size();
    }
    return stats_;
}

void printDispatcherSummary(std::ostream& os, const Dispatcher& d)
{
    const auto flags = os.flags();
    os.setf(std::ios::fixed, std::ios::floatfield);
    const auto prec = os.precision(9);

    std::vector<std::uint64_t> ids;
    ids.reserve(d.books().size());
    for (const auto& kv : d.books())
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    os << "Final BBO across " << ids.size() << " instrument(s):\n";
    for (auto iid : ids)
    {
        const auto& b = d.books().at(iid).bbo();
        os << "  iid=" << iid << "  ";
        if (b.bid_price)
            os << "bid=" << scaledToDouble(*b.bid_price) << " x " << b.bid_size;
        else
            os << "bid=-";
        os << "  ";
        if (b.ask_price)
            os << "ask=" << scaledToDouble(*b.ask_price) << " x " << b.ask_size;
        else
            os << "ask=-";
        os << "\n";
    }

    os.flags(flags);
    os.precision(prec);
}

} // namespace cmf
