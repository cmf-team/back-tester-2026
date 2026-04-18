#include "main/EventCollector.hpp"

namespace cmf {

EventCollector::EventCollector(std::size_t keep_first, std::size_t keep_last)
    : keep_first_(keep_first), keep_last_(keep_last) {
  first_.reserve(keep_first_);
}

void EventCollector::operator()(const MarketDataEvent &ev) {
  if (total_ == 0)
    first_ts_ = ev.timestamp;
  last_ts_ = ev.timestamp;
  ++total_;

  if (first_.size() < keep_first_)
    first_.push_back(ev);

  last_.push_back(ev);
  if (last_.size() > keep_last_)
    last_.pop_front();
}

void EventCollector::reset() {
  first_.clear();
  last_.clear();
  total_ = 0;
  first_ts_ = 0;
  last_ts_ = 0;
}

EventCollector &defaultEventCollector() {
  static EventCollector instance{};
  return instance;
}

} // namespace cmf
