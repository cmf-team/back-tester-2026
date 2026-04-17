#include "ingestion/HierarchyMerger.hpp"

namespace cmf {

HierarchyMerger::HierarchyMerger(std::vector<EventQueue*> leaf_inputs) {
    if (leaf_inputs.empty()) return;
    std::vector<EventQueue*> current = std::move(leaf_inputs);

    while (current.size() > 1) {
        std::vector<EventQueue*> next;
        for (std::size_t i = 0; i < current.size(); i += 2) {
            if (i + 1 >= current.size()) {
                next.push_back(current[i]);
                continue;
            }
            auto& q = node_queues_.emplace_back(std::make_unique<EventQueue>());
            specs_.push_back({current[i], current[i + 1], q.get()});
            next.push_back(q.get());
        }
        current = std::move(next);
    }

    final_queue_ = current[0];
}

HierarchyMerger::~HierarchyMerger() {
    for (auto& t : threads_)
        if (t.joinable()) t.join();
}

void HierarchyMerger::start() {
    for (auto& spec : specs_)
        threads_.emplace_back(merge_two, std::ref(*spec.left), std::ref(*spec.right),
                              std::ref(*spec.out));
}

void HierarchyMerger::join() {
    for (auto& t : threads_)
        if (t.joinable()) t.join();
}

void HierarchyMerger::merge_two(EventQueue& left, EventQueue& right, EventQueue& out) {
    static constexpr NanoTime S = MarketDataEvent::SENTINEL;

    MarketDataEvent l = left.pop();
    MarketDataEvent r = right.pop();

    while (l.ts_recv != S || r.ts_recv != S) {
        if (l.ts_recv == S) {
            out.push(r);
            r = right.pop();
        } else if (r.ts_recv == S) {
            out.push(l);
            l = left.pop();
        } else if (l.ts_recv <= r.ts_recv) {
            out.push(l);
            l = left.pop();
        } else {
            out.push(r);
            r = right.pop();
        }
    }

    MarketDataEvent sentinel{};
    sentinel.ts_recv = S;
    out.push(sentinel);
}

} // namespace cmf
