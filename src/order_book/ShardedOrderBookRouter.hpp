#pragma once

#include "MapOrderBook.hpp"
#include "OrderBookRouter.hpp"
#include "SimpleOrderBookRouter.hpp"
#include "common/BlockingQueue.hpp"
#include <array>
#include <future>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>

namespace cmf
{

template <typename BookType = MapOrderBook, std::size_t NumShards = 4>
class ShardedOrderBookRouter
    : public OrderBookRouter<ShardedOrderBookRouter<BookType, NumShards>>
{
    friend class OrderBookRouter<ShardedOrderBookRouter<BookType, NumShards>>;

  private:
    using ShardItem =
        std::variant<MarketDataEvent, std::shared_ptr<std::promise<void>>>;

    struct ItemVisitor
    {
        SimpleOrderBookRouter<BookType>& router;

        void operator()(const MarketDataEvent& e) const { router.apply(e); }

        void operator()(const std::shared_ptr<std::promise<void>>& p) const
        {
            p->set_value();
        }
    };

    std::array<BlockingQueue<ShardItem>, NumShards> queues_;
    std::array<SimpleOrderBookRouter<BookType>, NumShards> shard_routers_;
    std::array<std::thread, NumShards> workers_;
    std::unordered_map<uint64_t, uint32_t> order_to_instrument_;
    bool closed_ = false;

  public:
    ShardedOrderBookRouter()
    {
        for (std::size_t i = 0; i < NumShards; ++i)
        {
            workers_[i] = std::thread([this, i]
                                      { worker_loop(i); });
        }
    }

    ~ShardedOrderBookRouter() { close(); }

    ShardedOrderBookRouter(const ShardedOrderBookRouter&) = delete;
    ShardedOrderBookRouter& operator=(const ShardedOrderBookRouter&) = delete;
    ShardedOrderBookRouter(ShardedOrderBookRouter&&) = delete;
    ShardedOrderBookRouter& operator=(ShardedOrderBookRouter&&) = delete;

    void close() noexcept
    {
        if (closed_)
            return;
        closed_ = true;
        for (auto& q : queues_)
            q.close();
        for (auto& t : workers_)
            if (t.joinable())
                t.join();
    }

  private:
    void worker_loop(std::size_t shard_idx)
    {
        auto& queue = queues_[shard_idx];
        auto& router = shard_routers_[shard_idx];

        while (queue.pop(
            [&](ShardItem&& item)
            { std::visit(ItemVisitor{router}, item); }))
        {
        }
    }

    void sync_all()
    {
        if (closed_)
            return;

        std::vector<std::future<void>> futs;
        futs.reserve(NumShards);

        for (auto& q : queues_)
        {
            auto p = std::make_shared<std::promise<void>>();
            futs.push_back(p->get_future());
            try
            {
                q.push(ShardItem{p});
            }
            catch (const std::runtime_error&)
            {
                // Queue is closed, no point waiting
                return;
            }
        }

        for (auto& f : futs)
            f.wait();
    }

    void apply_impl(const MarketDataEvent& e)
    {
        if (e.instrument_id != 0 && e.order_id != 0)
            order_to_instrument_[e.order_id] = e.instrument_id;

        uint32_t instr_id = 0;
        if (e.instrument_id != 0)
        {
            instr_id = e.instrument_id;
        }
        else if (e.order_id != 0)
        {
            auto it = order_to_instrument_.find(e.order_id);
            if (it != order_to_instrument_.end())
            {
                instr_id = it->second;
            }
        }

        if (instr_id == 0 && e.order_id != 0) [[unlikely]]
        {
            throw std::runtime_error(
                "ShardedOrderBookRouter: cannot resolve instrument_id for order_id " +
                std::to_string(e.order_id));
        }

        std::size_t shard = instr_id % NumShards;
        queues_[shard].push(ShardItem{e});
    }

    void print_snapshot_impl(std::ostream& out,
                             const size_t group_by_levels) const
    {
        const_cast<ShardedOrderBookRouter*>(this)->sync_all();

        for (std::size_t i = 0; i < NumShards; ++i)
        {
            shard_routers_[i].print_snapshot(out, group_by_levels);
        }
    }

    void print_best_bid_ask_impl(std::ostream& out) const
    {
        const_cast<ShardedOrderBookRouter*>(this)->sync_all();

        out << "\n// ====== Final Best Bid/Ask ======\n";
        for (std::size_t i = 0; i < NumShards; ++i)
        {
            shard_routers_[i].print_best_bid_ask(out);
        }
        out << "// ====== End Best Bid/Ask ======\n";
    }

    [[nodiscard]] std::string
    snapshot_as_string_impl(const std::size_t group_by_levels) const
    {
        const_cast<ShardedOrderBookRouter*>(this)->sync_all();

        std::ostringstream oss;
        for (std::size_t i = 0; i < NumShards; ++i)
        {
            oss << shard_routers_[i].snapshot_as_string(group_by_levels);
        }
        return std::move(oss).str();
    }
};

} // namespace cmf
