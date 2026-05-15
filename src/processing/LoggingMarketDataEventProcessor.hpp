#pragma once

#include "processing/IMarketDataEventProcessor.hpp"

#include <cstddef>
#include <iosfwd>

namespace md {

class LoggingMarketDataEventProcessor final : public IMarketDataEventProcessor {
public:
    explicit LoggingMarketDataEventProcessor(std::ostream& out, std::size_t max_events_to_print = 10);

    void processMarketDataEvent(const MarketDataEvent& event) override;

    [[nodiscard]] std::size_t processedCount() const;
    [[nodiscard]] std::size_t printedCount() const;

private:
    std::ostream& out_;
    std::size_t max_events_to_print_{};
    std::size_t processed_count_{};
    std::size_t printed_count_{};
};

} // namespace md
