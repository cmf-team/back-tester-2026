#pragma once

#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include "ingestion/Merger.hpp"

#include <deque>
#include <optional>
#include <thread>
#include <vector>

namespace cmf
{

template <template <typename> typename InputQueueImpl = BlockingQueue,
          template <typename> typename OutputQueueImpl = InputQueueImpl>
class HierarchyMerger
    : public Merger<HierarchyMerger<InputQueueImpl, OutputQueueImpl>,
                    InputQueueImpl, OutputQueueImpl>
{
    using Base = Merger<HierarchyMerger<InputQueueImpl, OutputQueueImpl>,
                        InputQueueImpl, OutputQueueImpl>;
    using InputQueue = typename Base::InputQueue;
    using OutputQueue = typename Base::OutputQueue;
    friend class Merger<HierarchyMerger<InputQueueImpl, OutputQueueImpl>,
                        InputQueueImpl, OutputQueueImpl>;

  public:
    using Base::Base;

    void run_impl() const
    {
        std::vector<InputQueue*> cur;
        cur.reserve(this->file_queues_.size());
        for (std::size_t i = 0; i < this->file_queues_.size(); ++i)
            cur.push_back(&this->file_queues_[i]);

        if (cur.empty())
        {
            this->output_.close();
            return;
        }

        std::deque<InputQueue> pool;
        std::vector<std::thread> workers;

        // Pair up into intermediate BlockingQueues until 2 (or 1) remain.
        while (cur.size() > 2)
        {
            std::vector<InputQueue*> nxt;
            nxt.reserve((cur.size() + 1) / 2);

            for (std::size_t i = 0; i < cur.size(); i += 2)
            {
                if (i + 1 == cur.size())
                {
                    nxt.push_back(cur[i]);
                    continue;
                }
                InputQueue* out_q = &pool.emplace_back();
                nxt.push_back(out_q);

                auto* q0 = cur[i];
                auto* q1 = cur[i + 1];
                workers.emplace_back([q0, q1, out_q]()
                                     {
          merge_two(*q0, *q1, *out_q);
          out_q->close(); });
            }

            cur = std::move(nxt);
        }

        // Final merge into OutputQueue (may differ from InputQueue).
        if (cur.size() == 2)
        {
            auto* q0 = cur[0];
            auto* q1 = cur[1];
            workers.emplace_back([q0, q1, &out = this->output_]()
                                 {
        merge_two(*q0, *q1, out);
        out.close(); });
        }
        else
        {
            drain(*cur[0], this->output_);
            this->output_.close();
        }

        for (auto& t : workers)
            t.join();
    }

  private:
    template <typename Q>
    static bool pop_one(Q& q, std::optional<MarketDataEvent>& out)
    {
        return q.pop([&](MarketDataEvent&& e)
                     { out = e; });
    }

    template <typename OutQ>
    static void merge_two(InputQueue& a, InputQueue& b, OutQ& out)
    {
        std::optional<MarketDataEvent> ea, eb;

        (void)pop_one(a, ea);
        (void)pop_one(b, eb);

        while (ea || eb)
        {
            if (ea && eb)
            {
                if (ea->ts_recv > eb->ts_recv)
                {
                    out.push(*eb);
                    eb.reset();
                    (void)pop_one(b, eb);
                }
                else
                {
                    out.push(*ea);
                    ea.reset();
                    (void)pop_one(a, ea);
                }
            }
            else if (ea)
            {
                out.push(*ea);
                ea.reset();
                (void)pop_one(a, ea);
            }
            else
            {
                out.push(*eb);
                eb.reset();
                (void)pop_one(b, eb);
            }
        }
    }

    template <typename OutQ>
    static void drain(InputQueue& q, OutQ& out)
    {
        std::optional<MarketDataEvent> e;
        while (pop_one(q, e))
        {
            out.push(*e);
            e.reset();
        }
    }
};

} // namespace cmf
