#include "Merger.hpp"

#include <queue>
#include <thread>

namespace {


void binaryMerge(
    cmf::ThreadSafeQueue<cmf::MarketDataEvent>& left,
    cmf::ThreadSafeQueue<cmf::MarketDataEvent>& right,
    cmf::ThreadSafeQueue<cmf::MarketDataEvent>& out
) {
    cmf::MarketDataEvent leftEvent, rightEvent;
    bool hasLeft = left.pop(leftEvent);
    bool hasRight = right.pop(rightEvent);

    while (hasLeft && hasRight) {
        if (leftEvent.ts_recv <= rightEvent.ts_recv) {
            out.push(leftEvent);
            hasLeft = left.pop(leftEvent);
        } else {
            out.push(rightEvent);
            hasRight = right.pop(rightEvent);
        }
    }

   
    while (hasLeft) {
        out.push(leftEvent);
        hasLeft = left.pop(leftEvent);
    }
    while (hasRight) {
        out.push(rightEvent);
        hasRight = right.pop(rightEvent);
    }

    out.setDone();
}

} 

namespace cmf {

size_t hierarchyMerge(
    std::vector<ThreadSafeQueue<MarketDataEvent>>& queues,
    std::function<void(const MarketDataEvent&)> consumer
) {

    std::vector<ThreadSafeQueue<MarketDataEvent>*> current;
    for (auto& q : queues) {
        current.push_back(&q);
    }


    std::vector<std::unique_ptr<ThreadSafeQueue<MarketDataEvent>>> intermediates;
    std::vector<std::thread> mergeThreads;


    while (current.size() > 1) {
        std::vector<ThreadSafeQueue<MarketDataEvent>*> next;

        for (size_t i = 0; i + 1 < current.size(); i += 2) {
            auto merged = std::make_unique<ThreadSafeQueue<MarketDataEvent>>();
            auto* mergedPtr = merged.get();
            auto* leftPtr = current[i];
            auto* rightPtr = current[i + 1];

            mergeThreads.emplace_back([leftPtr, rightPtr, mergedPtr] {
                binaryMerge(*leftPtr, *rightPtr, *mergedPtr);
            });

            next.push_back(mergedPtr);
            intermediates.push_back(std::move(merged));
        }


        if (current.size() % 2 != 0) {
            next.push_back(current.back());
        }

        current = next;
    }


    size_t count = 0;
    MarketDataEvent event;
    while (current[0]->pop(event)) {
        consumer(event);
        ++count;
    }


    for (auto& t : mergeThreads) {
        t.join();
    }

    return count;
}

} // namespace cmf
