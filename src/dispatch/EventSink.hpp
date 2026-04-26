#pragma once

#include "common/MarketDataEvent.hpp"
#include "lob/LimitOrderBook.hpp"

namespace cmf {

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void on_event(const MarketDataEvent&, const LimitOrderBook&) {}
    virtual void on_trade(const MarketDataEvent&) {}
    virtual void on_clear(uint32_t /*instrument_id*/) {}
};

class NullSink : public EventSink {};

} // namespace cmf
