#pragma once
#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include "MarketDataEvent.hpp"

// ── Hierarchy Merger (multi-level tree-based merge) ────────────────────────
// Builds a binary tree of pair-wise mergers.
// Level 0: individual file streams
// Level 1: pairs merged into intermediate streams
// Level 2: pairs of level-1 merged again … until 1 stream remains
//
// Each internal node is a mini min-heap of 2 children.
// For N=20 files the tree has depth ~5 (log2(20)), so each event
// travels through ~5 comparisons — similar asymptotic cost to FlatMerger
// but with better cache locality for large N because each heap is tiny.

class HierarchyMerger {
public:
    using Stream = std::vector<MarketDataEvent>;

    explicit HierarchyMerger(std::vector<Stream> streams) {
        // Wrap each stream in a leaf node
        std::vector<std::shared_ptr<Node>> nodes;
        for (auto& s : streams)
            nodes.push_back(std::make_shared<LeafNode>(std::move(s)));

        // Reduce pairwise until one root remains
        while (nodes.size() > 1) {
            std::vector<std::shared_ptr<Node>> next;
            for (std::size_t i = 0; i < nodes.size(); i += 2) {
                if (i + 1 < nodes.size())
                    next.push_back(std::make_shared<MergeNode>(nodes[i], nodes[i+1]));
                else
                    next.push_back(nodes[i]); // odd one out — pass through
            }
            nodes = std::move(next);
        }
        root_ = nodes.empty() ? nullptr : nodes[0];
    }

    bool next(MarketDataEvent& out) {
        if (!root_) return false;
        return root_->next(out);
    }

    bool empty() const { return !root_ || root_->peek() == nullptr; }

private:
    // ── Abstract node ──────────────────────────────────────────────────────
    struct Node {
        virtual ~Node() = default;
        virtual bool next(MarketDataEvent& out) = 0;
        virtual const MarketDataEvent* peek() const = 0; // nullptr if exhausted
    };

    // ── Leaf: wraps one sorted stream ──────────────────────────────────────
    struct LeafNode : Node {
        explicit LeafNode(Stream s) : stream_(std::move(s)) {}
        bool next(MarketDataEvent& out) override {
            if (idx_ >= stream_.size()) return false;
            out = stream_[idx_++];
            return true;
        }
        const MarketDataEvent* peek() const override {
            return idx_ < stream_.size() ? &stream_[idx_] : nullptr;
        }
        Stream      stream_;
        std::size_t idx_{0};
    };

    // ── Internal node: merges two children ────────────────────────────────
    struct MergeNode : Node {
        MergeNode(std::shared_ptr<Node> l, std::shared_ptr<Node> r)
            : left_(std::move(l)), right_(std::move(r))
        {
            // Prime both sides
            advance(left_,  lbuf_, lvalid_);
            advance(right_, rbuf_, rvalid_);
        }

        bool next(MarketDataEvent& out) override {
            if (!lvalid_ && !rvalid_) return false;
            if (!lvalid_) { out = rbuf_; advance(right_, rbuf_, rvalid_); return true; }
            if (!rvalid_) { out = lbuf_; advance(left_,  lbuf_, lvalid_); return true; }
            if (lbuf_.ts <= rbuf_.ts) {
                out = lbuf_; advance(left_,  lbuf_, lvalid_);
            } else {
                out = rbuf_; advance(right_, rbuf_, rvalid_);
            }
            return true;
        }

        const MarketDataEvent* peek() const override {
            if (!lvalid_ && !rvalid_) return nullptr;
            if (!lvalid_) return &rbuf_;
            if (!rvalid_) return &lbuf_;
            return lbuf_.ts <= rbuf_.ts ? &lbuf_ : &rbuf_;
        }

    private:
        static void advance(std::shared_ptr<Node>& node,
                            MarketDataEvent& buf, bool& valid) {
            valid = node->next(buf);
        }

        std::shared_ptr<Node> left_, right_;
        MarketDataEvent       lbuf_{}, rbuf_{};
        bool                  lvalid_{false}, rvalid_{false};
    };

    std::shared_ptr<Node> root_;
};
