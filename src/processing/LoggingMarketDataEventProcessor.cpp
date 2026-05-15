#include "processing/LoggingMarketDataEventProcessor.hpp"

#include "domain/MarketDataEvent.hpp"

#include <iostream>

namespace md {

LoggingMarketDataEventProcessor::LoggingMarketDataEventProcessor(
    std::ostream& out,
    std::size_t max_events_to_print
) : out_(out), max_events_to_print_(max_events_to_print) {}

void LoggingMarketDataEventProcessor::processMarketDataEvent(const MarketDataEvent& event) {
    if (printed_count_ < max_events_to_print_) {
        out_ << formatEventFields(event) << '\n';
        ++printed_count_;
    }

    ++processed_count_;
}

std::size_t LoggingMarketDataEventProcessor::processedCount() const {
    return processed_count_;
}

std::size_t LoggingMarketDataEventProcessor::printedCount() const {
    return printed_count_;
}

} // namespace md
