#pragma once

#include "LimitOrderBook.hpp"
#include "MarketDataEvent.hpp"
#include "common/BasicTypes.hpp"

#include <unordered_map>

namespace cmf {

// what we remember about each resting order so we can later cancel/modify/fill it
struct ResidentOrder {
    uint32_t instrument_id = 0;
    Side     side          = Side::None;
    Price    price         = 0.0;
    Quantity size          = 0.0;  // remaining size
};

// global dispatcher: receives events in chronological order, maintains
// one LOB per instrument and one map of resident orders.
class Dispatcher {
public:
    void process(const MarketDataEvent& event);

    size_t bookCount() const;
    size_t orderCount() const;
    size_t eventsProcessed() const;

    // returns nullptr if no book exists for this instrument
    const LimitOrderBook* book(uint32_t instrument_id) const;

    // most active instrument (most events seen) — useful for end-of-run stats
    uint32_t mostActiveInstrument() const;

private:
    std::unordered_map<uint32_t, LimitOrderBook> books_;
    std::unordered_map<OrderId, ResidentOrder>   orders_;
    std::unordered_map<uint32_t, size_t>         eventsPerInstrument_;
    size_t eventCount_ = 0;
};

} // namespace cmf
