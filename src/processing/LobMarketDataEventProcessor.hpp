#pragma once

#include "book/BookManager.hpp"
#include "processing/AsyncSnapshotWriter.hpp"
#include "processing/IMarketDataEventProcessor.hpp"

#include <cstddef>
#include <iosfwd>

namespace md {

struct LobProcessorConfig {
    std::size_t snapshot_depth = 5;
    std::size_t snapshot_interval_events = 100'000;
    std::size_t max_snapshots = 5;
    SnapshotWriterMode snapshot_writer_mode = SnapshotWriterMode::Sync;
};

class LobMarketDataEventProcessor final : public IMarketDataEventProcessor {
public:
    explicit LobMarketDataEventProcessor(
        std::ostream& out,
        LobProcessorConfig config = {}
    );
    LobMarketDataEventProcessor(
        std::ostream& out,
        std::ostream& snapshot_out,
        LobProcessorConfig config
    );

    void processMarketDataEvent(const MarketDataEvent& event) override;

    [[nodiscard]] const BookManager& books() const noexcept;
    [[nodiscard]] std::size_t processedCount() const noexcept;
    [[nodiscard]] std::size_t snapshotCount() const noexcept;
    [[nodiscard]] std::size_t snapshotWrittenCount() const noexcept;

    void finishSnapshots();
    void printFinalSummary() const;
    void printFinalSummary(std::ostream& out) const;

private:
    void maybePrintSnapshot(std::uint64_t timestamp);

    std::ostream& out_;
    LobProcessorConfig config_;
    BookManager book_manager_;
    AsyncSnapshotWriter snapshot_writer_;
    std::size_t processed_count_{};
    std::size_t snapshot_count_{};
};

} // namespace md
