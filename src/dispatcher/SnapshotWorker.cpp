#include "dispatcher/SnapshotWorker.hpp"

#include "market_data/MarketDataEvent.hpp"

#include <ios>
#include <ostream>
#include <thread>
#include <utility>

namespace cmf
{

namespace
{

double scaledToDouble(std::int64_t scaled)
{
    return static_cast<double>(scaled) /
           static_cast<double>(MarketDataEvent::kPriceScale);
}

void writeTask(std::ostream& os, const SnapshotTask& t)
{
    const auto flags = os.flags();
    os.setf(std::ios::fixed, std::ios::floatfield);
    const auto prec = os.precision(9);

    os << "snapshot @ event=" << t.event_index << " (" << t.entries.size()
       << " instruments)\n";
    for (const auto& e : t.entries)
    {
        os << "  iid=" << e.instrument_id << "  ";
        if (e.bbo.bid_price)
            os << "bid=" << scaledToDouble(*e.bbo.bid_price) << " x " << e.bbo.bid_size;
        else
            os << "bid=-";
        os << "  ";
        if (e.bbo.ask_price)
            os << "ask=" << scaledToDouble(*e.bbo.ask_price) << " x " << e.bbo.ask_size;
        else
            os << "ask=-";
        os << "\n";
    }
    os.flush();

    os.flags(flags);
    os.precision(prec);
}

} // namespace

SnapshotWorker::SnapshotWorker(std::ostream* out, std::size_t queue_capacity)
    : out_(out), queue_(queue_capacity)
{
    if (out_)
    {
        thread_ = std::thread([this]
                              { run(); });
    }
}

SnapshotWorker::~SnapshotWorker()
{
    try
    {
        stop();
    }
    catch (...)
    {
        // Best-effort shutdown in destructor; never throw.
    }
}

void SnapshotWorker::enqueue(SnapshotTask task)
{
    if (!out_)
        return; // disabled — drop snapshots silently
    while (!queue_.push(std::move(task)))
    {
        std::this_thread::yield();
    }
}

void SnapshotWorker::stop()
{
    if (stopped_)
        return;
    stopped_ = true;
    queue_.close();
    if (thread_.joinable())
        thread_.join();
}

void SnapshotWorker::run()
{
    SnapshotTask task;
    while (!queue_.done())
    {
        if (queue_.pop(task))
        {
            if (out_)
                writeTask(*out_, task);
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

} // namespace cmf
