#include "TestSupport.hpp"

#include "runners/RunResult.hpp"

#include <limits>

namespace md::test {

void testMarketDataEventFormattingAndOrdering() {
    require(toChar(Side::Ask) == 'A', "ask side char");
    require(toChar(Side::Bid) == 'B', "bid side char");
    require(toChar(Action::Trade) == 'T', "trade action char");
    require(sideName(Side::None) == "None", "none side name");
    require(actionName(Action::Cancel) == "Cancel", "cancel action name");

    require(formatPrice(1'250'000'000) == "1.250000000", "fixed price formatting");
    require(formatPrice(std::numeric_limits<std::int64_t>::max()) == "UNDEF", "undef price formatting");

    MarketDataEvent first;
    first.timestamp = 100;
    first.source_file_id = 1;
    first.source_sequence = 2;

    MarketDataEvent second = first;
    second.source_file_id = 2;
    second.source_sequence = 1;

    MarketDataEvent third = first;
    third.timestamp = 101;

    require(eventComesBefore(first, second), "source_file_id tie-breaker");
    require(eventComesBefore(second, third), "timestamp ordering wins over source metadata");
    requireContains(formatEventFields(first), "timestamp=100", "event fields include timestamp");
}

void testProcessingSummaryChronologicalViolations() {
    ProcessingSummary summary;

    MarketDataEvent first;
    first.timestamp = 200;
    first.source_file_id = 0;
    first.source_sequence = 1;

    MarketDataEvent second;
    second.timestamp = 100;
    second.source_file_id = 0;
    second.source_sequence = 2;

    summary.observe(first);
    summary.observe(second);

    require(summary.total_messages_processed == 2, "summary observed both events");
    require(summary.chronological_violations == 1, "summary detects chronological violation");
    require(summary.first_timestamp == 200, "summary first timestamp preserved");
    require(summary.last_timestamp == 100, "summary last timestamp preserved");
}

} // namespace md::test
