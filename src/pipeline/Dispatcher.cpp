#include "pipeline/Dispatcher.hpp"

namespace cmf {

bool Dispatcher::step() {
  MarketDataEvent ev;
  if (!source_.next(ev))
    return false;

  if (stats_.events_in == 0)
    stats_.first_ts = ev.timestamp;
  stats_.last_ts = ev.timestamp;
  ++stats_.events_in;

  switch (registry_.apply(ev)) {
  case InstrumentBookRegistry::RouteResult::Applied:
    ++stats_.events_routed;
    break;
  case InstrumentBookRegistry::RouteResult::UnknownOrder:
    ++stats_.events_unknown;
    break;
  case InstrumentBookRegistry::RouteResult::Unroutable:
    ++stats_.events_unroutable;
    break;
  }

  if (snapshot_every_ != 0 && on_snapshot_ &&
      (stats_.events_in % snapshot_every_) == 0) {
    on_snapshot_(stats_.events_in, stats_.last_ts);
  }
  return true;
}

DispatcherStats Dispatcher::run() {
  while (step()) {
  }
  return stats_;
}

} // namespace cmf
