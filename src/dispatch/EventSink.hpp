#pragma once

#include "common/MarketDataEvent.hpp"
#include "lob/LimitOrderBook.hpp"

#include <cstdint>
#include <vector>

namespace cmf {

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void on_event(const MarketDataEvent&, const LimitOrderBook&) {}
    virtual void on_trade(const MarketDataEvent&) {}
    virtual void on_clear(uint32_t /*instrument_id*/) {}
};

class NullSink : public EventSink {};

class MultiSink : public EventSink {
public:
    void add(EventSink* s) { sinks_.push_back(s); }
    void on_event(const MarketDataEvent& e, const LimitOrderBook& b) override {
        for (auto* s : sinks_) s->on_event(e, b);
    }
    void on_trade(const MarketDataEvent& e) override {
        for (auto* s : sinks_) s->on_trade(e);
    }
    void on_clear(uint32_t inst) override {
        for (auto* s : sinks_) s->on_clear(inst);
    }
private:
    std::vector<EventSink*> sinks_;
};

} // namespace cmf
