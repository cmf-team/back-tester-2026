#pragma once

#include <memory>
#include <thread>
#include <vector>
#include <limits>

namespace cmf {

class EventHierarchyMerger {
public:
    explicit EventHierarchyMerger(std::vector<SpscQueue<MarketDataEvent>*> leaf_inputs) {
        if (leaf_inputs.empty()) return;

        if (leaf_inputs.size() == 1) {
            single_ = leaf_inputs[0];
            return;
        }

        std::vector<SpscQueue<MarketDataEvent>*> current = std::move(leaf_inputs);

        while (current.size() > 1) {
            std::vector<SpscQueue<MarketDataEvent>*> next;

            for (size_t i = 0; i < current.size(); i += 2) {
                if (i + 1 >= current.size()) {
                    next.push_back(current[i]);
                    continue;
                }

                auto q = std::make_unique<SpscQueue<MarketDataEvent>>();
                SpscQueue<MarketDataEvent>* out = q.get();

                specs_.push_back({current[i], current[i + 1], out});
                node_queues_.push_back(std::move(q));

                next.push_back(out);
            }

            current = std::move(next);
        }

        if (current.size() == 1) {
            root_left_ = current[0];
            root_right_ = nullptr;
        }
    }

    ~EventHierarchyMerger() {
        join();
    }

    void start() {
        for (auto& s : specs_) {
            threads_.emplace_back([&]() {
                merge_two(*s.left, *s.right, *s.out);
            });
        }
    }

    void join() {
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }

    bool next(MarketDataEvent& out) {
        if (single_) {
            single_->pop(out);
            return out.ts_recv != MarketDataEvent::SENTINEL;
        }

        if (!root_left_) return false;

        if (!primed_) {
            root_left_->pop(buf_left_);
            if (root_right_) root_right_->pop(buf_right_);
            primed_ = true;
        }

        if (!root_right_) {
            out = buf_left_;
            root_left_->pop(buf_left_);
            return out.ts_recv != MarketDataEvent::SENTINEL;
        }

        if (buf_left_.ts_recv <= buf_right_.ts_recv) {
            out = buf_left_;
            root_left_->pop(buf_left_);
        } else {
            out = buf_right_;
            root_right_->pop(buf_right_);
        }

        return out.ts_recv != MarketDataEvent::SENTINEL;
    }

private:
    struct MergeSpec {
        SpscQueue<MarketDataEvent>* left;
        SpscQueue<MarketDataEvent>* right;
        SpscQueue<MarketDataEvent>* out;
    };

    static void merge_two(SpscQueue<MarketDataEvent>& left, SpscQueue<MarketDataEvent>& right, SpscQueue<MarketDataEvent>& out) {
        MarketDataEvent l{}, r{};
        left.pop(l);
        right.pop(r);

        while (true) {
            if (l.ts_recv <= r.ts_recv) {
                out.push(l);
                if (l.ts_recv == MarketDataEvent::SENTINEL) break;
                left.pop(l);
            } else {
                out.push(r);
                if (r.ts_recv == MarketDataEvent::SENTINEL) break;
                right.pop(r);
            }
        }

        MarketDataEvent sentinel{};
        sentinel.ts_recv = MarketDataEvent::SENTINEL;
        out.push(sentinel);
    }

    std::vector<std::unique_ptr<SpscQueue<MarketDataEvent>>> node_queues_;
    std::vector<MergeSpec>                   specs_;
    std::vector<std::thread>                 threads_;

    SpscQueue<MarketDataEvent>*     root_left_  = nullptr;
    SpscQueue<MarketDataEvent>*     root_right_ = nullptr;
    SpscQueue<MarketDataEvent>*     single_     = nullptr;
    MarketDataEvent buf_left_{};
    MarketDataEvent buf_right_{};
    bool            primed_     = false;
};

} // namespace cmf