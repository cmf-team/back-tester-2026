#pragma once

#include "domain/MarketDataEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace md {

struct ParseDiagnostics {
    std::size_t total_lines_read{};

    void add(const ParseDiagnostics& other) {
        total_lines_read += other.total_lines_read;
    }
};

struct ProcessingSummary {
    std::size_t total_messages_processed{};
    std::size_t chronological_violations{};

    std::optional<std::uint64_t> first_timestamp;
    std::optional<std::uint64_t> last_timestamp;
    std::optional<MarketDataEvent> previous_event;

    std::vector<MarketDataEvent> first_events;
    std::deque<MarketDataEvent> last_events;

    void observe(const MarketDataEvent& event) {
        if (previous_event.has_value() && eventComesBefore(event, *previous_event)) {
            ++chronological_violations;
        }

        previous_event = event;

        if (!first_timestamp.has_value()) {
            first_timestamp = event.timestamp;
        }

        last_timestamp = event.timestamp;

        if (first_events.size() < 10) {
            first_events.push_back(event);
        }

        if (last_events.size() == 10) {
            last_events.pop_front();
        }

        last_events.push_back(event);

        ++total_messages_processed;
    }
};

struct RunResult {
    std::string strategy_name;
    ProcessingSummary summary;
    ParseDiagnostics diagnostics;
    double wall_clock_seconds{};
};

struct BenchmarkResult {
    RunResult result;
    std::string input_format{"json"};
    std::string processor{"logging"};
    std::size_t unresolved_events{};
    std::size_t lob_workers{};
    std::string lob_digest;
};

} // namespace md
