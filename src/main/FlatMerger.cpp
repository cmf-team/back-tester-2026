#include "Merger.hpp"

#include <queue>

namespace {

struct HeapEntry {
    cmf::MarketDataEvent event;
    size_t queueIndex;
};

auto cmp = [](const HeapEntry& a, const HeapEntry& b) {
    return a.event.ts_recv > b.event.ts_recv;
};

} 

namespace cmf {

size_t flatMerge(
    std::vector<ThreadSafeQueue<MarketDataEvent>>& queues,
    std::function<void(const MarketDataEvent&)> consumer
) {
    
    std::priority_queue<HeapEntry, std::vector<HeapEntry>, decltype(cmp)> heap(cmp);

    
    for (size_t i = 0; i < queues.size(); ++i) {
        MarketDataEvent event;
        if (queues[i].pop(event)) {
            heap.push({event, i});
        }
    }

    size_t count = 0;

    
    while (!heap.empty()) {
        HeapEntry top = heap.top();
        heap.pop();

        consumer(top.event);
        ++count;

        
        MarketDataEvent next;
        if (queues[top.queueIndex].pop(next)) {
            heap.push({next, top.queueIndex});
        }
    }

    return count;
}

} // namespace cmf
