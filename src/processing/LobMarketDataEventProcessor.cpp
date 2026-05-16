#include "processing/LobMarketDataEventProcessor.hpp"

#include <ostream>

namespace md {

LobMarketDataEventProcessor::LobMarketDataEventProcessor(
    std::ostream& out,
    LobProcessorConfig config
) : LobMarketDataEventProcessor(out, out, config) {}

LobMarketDataEventProcessor::LobMarketDataEventProcessor(
    std::ostream& out,
    std::ostream& snapshot_out,
    LobProcessorConfig config
) : out_(out),
    config_(config),
    snapshot_writer_(snapshot_out, config.snapshot_writer_mode) {}

void LobMarketDataEventProcessor::processMarketDataEvent(const MarketDataEvent& event) {
    book_manager_.apply(event);
    ++processed_count_;
    maybePrintSnapshot(event.timestamp);
}

const BookManager& LobMarketDataEventProcessor::books() const noexcept {
    return book_manager_;
}

std::size_t LobMarketDataEventProcessor::processedCount() const noexcept {
    return processed_count_;
}

std::size_t LobMarketDataEventProcessor::snapshotCount() const noexcept {
    return snapshot_count_;
}

std::size_t LobMarketDataEventProcessor::snapshotWrittenCount() const noexcept {
    return snapshot_writer_.writtenCount();
}

void LobMarketDataEventProcessor::finishSnapshots() {
    snapshot_writer_.finish();
}

void LobMarketDataEventProcessor::printFinalSummary() const {
    printFinalSummary(out_);
}

void LobMarketDataEventProcessor::printFinalSummary(std::ostream& out) const {
    out << "Final LOB Summary\n";
    book_manager_.printFinalBestBidAsk(out);
}

void LobMarketDataEventProcessor::maybePrintSnapshot(std::uint64_t timestamp) {
    if (config_.snapshot_interval_events == 0 || snapshot_count_ >= config_.max_snapshots) {
        return;
    }

    if (processed_count_ % config_.snapshot_interval_events != 0) {
        return;
    }

    snapshot_writer_.write(book_manager_.snapshot(
        processed_count_,
        timestamp,
        config_.snapshot_depth
    ));
    ++snapshot_count_;
}

} // namespace md
