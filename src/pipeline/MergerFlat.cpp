#include "pipeline/MergerFlat.hpp"

#include <utility>

namespace cmf {

MergerFlat::MergerFlat(std::vector<EventSourcePtr> sources)
    : sources_(std::move(sources)),
      buffered_(sources_.size()),
      exhausted_(sources_.size(), false) {
  // Prime the heap with the first event from each source.
  for (std::size_t i = 0; i < sources_.size(); ++i)
    refill(i);
}

void MergerFlat::refill(std::size_t idx) {
  if (exhausted_[idx])
    return;
  if (!sources_[idx]->next(buffered_[idx])) {
    exhausted_[idx] = true;
    return;
  }
  heap_.push({buffered_[idx].timestamp, idx});
}

bool MergerFlat::next(MarketDataEvent &out) {
  if (heap_.empty())
    return false;
  const auto top = heap_.top();
  heap_.pop();
  out = std::move(buffered_[top.idx]);
  refill(top.idx);
  ++emitted_;
  return true;
}

} // namespace cmf
