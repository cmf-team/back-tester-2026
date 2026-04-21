#include "market_data/Merger.hpp"

#include <algorithm>
#include <utility>

namespace cmf {

// ---------------------------------------------------------------------------
// FlatMerger
// ---------------------------------------------------------------------------

FlatMerger::FlatMerger(std::vector<IEventSource *> sources)
    : sources_(std::move(sources)) {
  heap_.reserve(sources_.size());
  for (std::size_t i = 0; i < sources_.size(); ++i) {
    MarketDataEvent e;
    if (sources_[i]->pop(e)) {
      heap_.push_back({std::move(e), i});
      std::push_heap(heap_.begin(), heap_.end(), Greater{});
    }
  }
}

bool FlatMerger::next(MarketDataEvent &out) {
  if (heap_.empty())
    return false;

  std::pop_heap(heap_.begin(), heap_.end(), Greater{});
  HeapEntry &popped = heap_.back();
  out = std::move(popped.event);
  const std::size_t src_idx = popped.src_idx;
  heap_.pop_back();

  MarketDataEvent next_e;
  if (sources_[src_idx]->pop(next_e)) {
    heap_.push_back({std::move(next_e), src_idx});
    std::push_heap(heap_.begin(), heap_.end(), Greater{});
  }
  return true;
}

// ---------------------------------------------------------------------------
// HierarchyMerger::Node — abstract internal node, defined here so the
// destructor of the forward-declared Node can be instantiated.
// ---------------------------------------------------------------------------
class HierarchyMerger::Node {
public:
  Node() = default;
  virtual ~Node() = default;
  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;
  virtual bool pullNext(MarketDataEvent &out) = 0;
};

namespace {

class LeafNode final : public HierarchyMerger::Node {
public:
  explicit LeafNode(IEventSource *src) noexcept : src_(src) {}

  bool pullNext(MarketDataEvent &out) override { return src_->pop(out); }

private:
  IEventSource *src_;
};

class PairNode final : public HierarchyMerger::Node {
public:
  PairNode(std::unique_ptr<HierarchyMerger::Node> l,
           std::unique_ptr<HierarchyMerger::Node> r)
      : left_(std::move(l)), right_(std::move(r)) {
    left_valid_ = left_->pullNext(left_head_);
    right_valid_ = right_->pullNext(right_head_);
  }

  bool pullNext(MarketDataEvent &out) override {
    if (!left_valid_ && !right_valid_)
      return false;

    if (!right_valid_) {
      out = std::move(left_head_);
      left_valid_ = left_->pullNext(left_head_);
      return true;
    }
    if (!left_valid_) {
      out = std::move(right_head_);
      right_valid_ = right_->pullNext(right_head_);
      return true;
    }
    if (left_head_ < right_head_) {
      out = std::move(left_head_);
      left_valid_ = left_->pullNext(left_head_);
    } else {
      out = std::move(right_head_);
      right_valid_ = right_->pullNext(right_head_);
    }
    return true;
  }

private:
  std::unique_ptr<HierarchyMerger::Node> left_;
  std::unique_ptr<HierarchyMerger::Node> right_;
  MarketDataEvent left_head_;
  MarketDataEvent right_head_;
  bool left_valid_{false};
  bool right_valid_{false};
};

} // namespace

// ---------------------------------------------------------------------------
// HierarchyMerger
// ---------------------------------------------------------------------------

HierarchyMerger::HierarchyMerger(const std::vector<IEventSource *> &sources) {
  if (sources.empty())
    return;

  std::vector<std::unique_ptr<Node>> level;
  level.reserve(sources.size());
  for (IEventSource *s : sources)
    level.push_back(std::make_unique<LeafNode>(s));

  // Build bottom-up: pair adjacent nodes; odd node carries over.
  while (level.size() > 1) {
    std::vector<std::unique_ptr<Node>> next_level;
    next_level.reserve((level.size() + 1) / 2);
    for (std::size_t i = 0; i + 1 < level.size(); i += 2) {
      next_level.push_back(std::make_unique<PairNode>(std::move(level[i]),
                                                      std::move(level[i + 1])));
    }
    if (level.size() % 2 == 1)
      next_level.push_back(std::move(level.back()));
    level = std::move(next_level);
  }
  root_ = std::move(level[0]);
}

bool HierarchyMerger::next(MarketDataEvent &out) {
  if (!root_)
    return false;
  return root_->pullNext(out);
}

// Out-of-line dtor so the forward-declared Node type can be destroyed.
HierarchyMerger::~HierarchyMerger() = default;

} // namespace cmf
