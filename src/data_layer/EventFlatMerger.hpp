#pragma once
#include <vector>
#include <limits>
#include "common/Queue.hpp"
#include "common/MarketDataEvent.hpp"

namespace cmf {

class EventFlatMerger {
public:
    explicit EventFlatMerger(std::vector<SpscQueue<MarketDataEvent>*> inputs)
        : sources_(std::move(inputs))
    {
        heads_.resize(sources_.size());
        nextTs_.resize(sources_.size(), MarketDataEvent::SENTINEL);
    }

    void start() {
        active_ = 0;

        for (std::size_t i = 0; i < sources_.size(); ++i) {
            sources_[i]->pop(heads_[i]);
            nextTs_[i] = heads_[i].ts_recv;

            if (nextTs_[i] != MarketDataEvent::SENTINEL) {
                ++active_;
            }
        }
    }

    bool next(MarketDataEvent& out) {
        if (active_ == 0) return false;

        std::size_t bestIdx = invalid_index();
        NanoTime bestTs = std::numeric_limits<NanoTime>::max();

        for (std::size_t i = 0; i < nextTs_.size(); ++i) {
            const NanoTime ts = nextTs_[i];
            if (ts == MarketDataEvent::SENTINEL) continue;

            if (ts < bestTs) {
                bestTs = ts;
                bestIdx = i;
            }
        }

        if (bestIdx == invalid_index()) return false;

        out = heads_[bestIdx];

        sources_[bestIdx]->pop(heads_[bestIdx]);
        nextTs_[bestIdx] = heads_[bestIdx].ts_recv;

        if (nextTs_[bestIdx] == MarketDataEvent::SENTINEL) {
            --active_;
        }

        return true;
    }

    void join() noexcept {}

private:
    static constexpr std::size_t invalid_index() noexcept {
        return std::numeric_limits<std::size_t>::max();
    }

    std::vector<SpscQueue<MarketDataEvent>*> sources_;
    std::vector<MarketDataEvent> heads_;
    std::vector<NanoTime> nextTs_;
    std::size_t active_ = 0;
};

} // namespace cmf