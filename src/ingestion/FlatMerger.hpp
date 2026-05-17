#pragma once

#include "common/MarketDataEvent.hpp"
#include "common/Queue.hpp"
#include "ingestion/Merger.hpp"

#include <cstddef>
#include <queue>
#include <vector>

namespace cmf
{

template <template <typename> typename InputQueueImpl = BlockingQueue,
          template <typename> typename OutputQueueImpl = InputQueueImpl>
class FlatMerger : public Merger<FlatMerger<InputQueueImpl, OutputQueueImpl>,
                                 InputQueueImpl, OutputQueueImpl>
{
    using Base = Merger<FlatMerger<InputQueueImpl, OutputQueueImpl>,
                        InputQueueImpl, OutputQueueImpl>;
    using InputQueue = typename Base::InputQueue;
    using OutputQueue = typename Base::OutputQueue;
    friend class Merger<FlatMerger<InputQueueImpl, OutputQueueImpl>,
                        InputQueueImpl, OutputQueueImpl>;

  public:
    using Base::Base;

    void run_impl() const
    {
        merge_impl([this](const MarketDataEvent& e)
                   { this->output_.push(e); });
        this->output_.close();
    }

    template <typename Fn>
        requires std::invocable<Fn, const MarketDataEvent&>
    void run_with_callback(Fn&& callback) const
    {
        merge_impl(std::forward<Fn>(callback));
    }

  private:
    template <typename Fn>
    void merge_impl(Fn&& emit) const
    {
        const std::size_t N = this->file_queues_.size();
        std::vector<MarketDataEvent> buf(N);

        auto cmp = [](const HeapKey& a, const HeapKey& b) noexcept
        {
            return a.ts_recv > b.ts_recv;
        };
        std::priority_queue<HeapKey, std::vector<HeapKey>, decltype(cmp)> heap(cmp);

        for (std::size_t i = 0; i < N; ++i)
        {
            (void)this->file_queues_[i].pop([&](MarketDataEvent&& e)
                                            {
        buf[i] = e;
        heap.push({buf[i].ts_recv, i}); });
        }

        while (!heap.empty())
        {
            const HeapKey top = heap.top();
            heap.pop();
            emit(buf[top.file_idx]);

            (void)this->file_queues_[top.file_idx].pop([&](MarketDataEvent&& e)
                                                       {
        buf[top.file_idx] = std::move(e);
        heap.push({buf[top.file_idx].ts_recv, top.file_idx}); });
        }
    }

  private:
    struct HeapKey
    {
        NanoTime ts_recv;
        std::size_t file_idx;
    };
};

} // namespace cmf
