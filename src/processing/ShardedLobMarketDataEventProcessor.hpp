#pragma once

#include "book/BookSnapshot.hpp"
#include "processing/AsyncSnapshotWriter.hpp"
#include "processing/IMarketDataEventProcessor.hpp"
#include "processing/LobMarketDataEventProcessor.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace md {

class ShardedLobMarketDataEventProcessor final : public IMarketDataEventProcessor {
public:
    ShardedLobMarketDataEventProcessor(
        std::ostream& out,
        std::size_t worker_count,
        LobProcessorConfig config = {}
    );
    ShardedLobMarketDataEventProcessor(
        std::ostream& out,
        std::ostream& snapshot_out,
        std::size_t worker_count,
        LobProcessorConfig config
    );
    ~ShardedLobMarketDataEventProcessor() override;

    ShardedLobMarketDataEventProcessor(const ShardedLobMarketDataEventProcessor&) = delete;
    ShardedLobMarketDataEventProcessor& operator=(const ShardedLobMarketDataEventProcessor&) = delete;

    void processMarketDataEvent(const MarketDataEvent& event) override;

    [[nodiscard]] std::size_t workerCount() const noexcept;
    [[nodiscard]] std::size_t processedCount() const noexcept;
    [[nodiscard]] std::size_t snapshotCount() const noexcept;
    [[nodiscard]] std::size_t snapshotWrittenCount() const noexcept;

    [[nodiscard]] std::size_t unresolvedEvents();
    [[nodiscard]] std::string stableStateDigest();

    void finish();
    void finishSnapshots();
    void printFinalSummary();
    void printFinalSummary(std::ostream& out);

private:
    struct RouterOrder {
        std::uint64_t instrument_id{};
        std::uint64_t size{};
    };

    class Worker;

    [[nodiscard]] std::uint64_t resolveInstrumentId(const MarketDataEvent& event) const;
    [[nodiscard]] std::size_t workerIndex(std::uint64_t instrument_id) const noexcept;
    void updateRouterAndDispatch(MarketDataEvent event);
    void enqueueFullCancel(std::uint64_t instrument_id, std::uint64_t order_id, const MarketDataEvent& source);
    void eraseRouterOrdersForInstrument(std::uint64_t instrument_id);
    void maybePrintSnapshot(std::uint64_t timestamp);
    void drainWorkers();
    [[nodiscard]] BookManagerSnapshot mergedSnapshot(
        std::size_t event_count,
        std::uint64_t timestamp,
        std::size_t depth
    );
    [[nodiscard]] std::size_t totalUnresolvedEvents();

    std::ostream& out_;
    LobProcessorConfig config_;
    AsyncSnapshotWriter snapshot_writer_;
    std::vector<std::unique_ptr<Worker>> workers_;
    std::unordered_map<std::uint64_t, RouterOrder> router_orders_;
    std::size_t processed_count_{};
    std::size_t router_unresolved_events_{};
    std::size_t snapshot_count_{};
    bool finished_{false};
};

} // namespace md
