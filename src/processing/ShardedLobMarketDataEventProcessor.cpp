#include "processing/ShardedLobMarketDataEventProcessor.hpp"

#include "book/BookManager.hpp"
#include "concurrency/NonBlockingQueue.hpp"
#include "domain/MarketDataEvent.hpp"

#include <algorithm>
#include <atomic>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace md {
namespace {

bool hasValidSide(Side side) {
    return side == Side::Bid || side == Side::Ask;
}

bool hasValidPrice(std::int64_t price) {
    return price != std::numeric_limits<std::int64_t>::max();
}

bool hasValidRestingState(const MarketDataEvent& event) {
    return event.order_id != 0
        && hasValidSide(event.side)
        && hasValidPrice(event.price)
        && event.size > 0;
}

std::string formatOptionalPrice(std::optional<std::int64_t> price) {
    return price.has_value() ? formatPrice(*price) : "<none>";
}

} // namespace

class ShardedLobMarketDataEventProcessor::Worker {
public:
    Worker() {
        thread_ = std::thread([this] {
            workerLoop();
        });
    }

    ~Worker() {
        finish();
    }

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    void enqueue(MarketDataEvent event) {
        if (finish_requested_) {
            throw std::runtime_error("cannot enqueue LOB event after worker finish");
        }

        enqueued_count_.fetch_add(1, std::memory_order_release);
        queue_.push(WorkerItem::data(std::move(event)));
    }

    void drain() {
        queue_.flush();
        const auto target = enqueued_count_.load(std::memory_order_acquire);
        auto processed = processed_count_.load(std::memory_order_acquire);
        while (processed < target) {
            processed_count_.wait(processed, std::memory_order_acquire);
            processed = processed_count_.load(std::memory_order_acquire);
        }
    }

    void finish() {
        if (finish_requested_) {
            return;
        }

        finish_requested_ = true;
        queue_.push(WorkerItem::end());
        queue_.flush();

        if (thread_.joinable()) {
            thread_.join();
        }
    }

    [[nodiscard]] const BookManager& books() const noexcept {
        return books_;
    }

private:
    struct WorkerItem {
        MarketDataEvent event;
        bool end_of_stream{};

        static WorkerItem data(MarketDataEvent event) {
            return WorkerItem{
                .event = std::move(event),
                .end_of_stream = false,
            };
        }

        static WorkerItem end() {
            return WorkerItem{
                .event = {},
                .end_of_stream = true,
            };
        }
    };

    void workerLoop() {
        while (true) {
            WorkerItem item = queue_.pop();
            if (item.end_of_stream) {
                processed_count_.notify_all();
                return;
            }

            books_.apply(item.event);
            processed_count_.fetch_add(1, std::memory_order_release);
            processed_count_.notify_all();
        }
    }

    BookManager books_;
    NonBlockingQueue<WorkerItem> queue_{256};
    std::thread thread_;
    bool finish_requested_{false};
    alignas(64) std::atomic<std::size_t> enqueued_count_{0};
    alignas(64) std::atomic<std::size_t> processed_count_{0};
};

ShardedLobMarketDataEventProcessor::ShardedLobMarketDataEventProcessor(
    std::ostream& out,
    std::size_t worker_count,
    LobProcessorConfig config
) : ShardedLobMarketDataEventProcessor(out, out, worker_count, config) {}

ShardedLobMarketDataEventProcessor::ShardedLobMarketDataEventProcessor(
    std::ostream& out,
    std::ostream& snapshot_out,
    std::size_t worker_count,
    LobProcessorConfig config
) : out_(out),
    config_(config),
    snapshot_writer_(snapshot_out, config.snapshot_writer_mode) {
    if (worker_count == 0) {
        throw std::invalid_argument("LOB worker count must be greater than zero");
    }

    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.push_back(std::make_unique<Worker>());
    }
}

ShardedLobMarketDataEventProcessor::~ShardedLobMarketDataEventProcessor() {
    finish();
}

void ShardedLobMarketDataEventProcessor::processMarketDataEvent(const MarketDataEvent& event) {
    if (finished_) {
        throw std::runtime_error("cannot process LOB event after finish");
    }

    ++processed_count_;

    const auto instrument_id = resolveInstrumentId(event);
    if (instrument_id == 0) {
        ++router_unresolved_events_;
        maybePrintSnapshot(event.timestamp);
        return;
    }

    MarketDataEvent routed_event = event;
    routed_event.instrument_id = instrument_id;
    updateRouterAndDispatch(routed_event);
    maybePrintSnapshot(event.timestamp);
}

std::size_t ShardedLobMarketDataEventProcessor::workerCount() const noexcept {
    return workers_.size();
}

std::size_t ShardedLobMarketDataEventProcessor::processedCount() const noexcept {
    return processed_count_;
}

std::size_t ShardedLobMarketDataEventProcessor::snapshotCount() const noexcept {
    return snapshot_count_;
}

std::size_t ShardedLobMarketDataEventProcessor::snapshotWrittenCount() const noexcept {
    return snapshot_writer_.writtenCount();
}

std::size_t ShardedLobMarketDataEventProcessor::unresolvedEvents() {
    return totalUnresolvedEvents();
}

std::string ShardedLobMarketDataEventProcessor::stableStateDigest() {
    const auto snapshot = mergedSnapshot(
        processed_count_,
        0,
        std::numeric_limits<std::size_t>::max()
    );

    std::ostringstream out;
    out << "processed_events=" << processed_count_
        << ";unresolved_events=" << snapshot.unresolved_events
        << ";instrument_count=" << snapshot.instruments.size();

    for (const auto& instrument : snapshot.instruments) {
        out << "|instrument=" << instrument.instrument_id
            << ",orders=" << instrument.resting_orders
            << ",best_bid=" << formatOptionalPrice(instrument.best_bid)
            << ",best_ask=" << formatOptionalPrice(instrument.best_ask);

        out << ",bids=[";
        bool first = true;
        for (const auto& level : instrument.bids) {
            if (!first) {
                out << ';';
            }
            first = false;
            out << formatPrice(level.price) << 'x' << level.size;
        }
        out << "]";

        out << ",asks=[";
        first = true;
        for (const auto& level : instrument.asks) {
            if (!first) {
                out << ';';
            }
            first = false;
            out << formatPrice(level.price) << 'x' << level.size;
        }
        out << "]";
    }

    return out.str();
}

void ShardedLobMarketDataEventProcessor::finish() {
    if (finished_) {
        return;
    }

    for (auto& worker : workers_) {
        worker->finish();
    }
    snapshot_writer_.finish();
    finished_ = true;
}

void ShardedLobMarketDataEventProcessor::finishSnapshots() {
    snapshot_writer_.finish();
}

void ShardedLobMarketDataEventProcessor::printFinalSummary() {
    printFinalSummary(out_);
}

void ShardedLobMarketDataEventProcessor::printFinalSummary(std::ostream& out) {
    const auto snapshot = mergedSnapshot(
        processed_count_,
        0,
        std::numeric_limits<std::size_t>::max()
    );

    out << "Final LOB Summary\n"
        << "worker_count=" << workerCount() << '\n'
        << "instrument_count=" << snapshot.instruments.size() << '\n'
        << "processed_events=" << processed_count_ << '\n'
        << "unresolved_events=" << snapshot.unresolved_events << '\n';

    for (const auto& instrument : snapshot.instruments) {
        out << "instrument_id=" << instrument.instrument_id
            << " resting_orders=" << instrument.resting_orders
            << " best_bid=" << formatOptionalPrice(instrument.best_bid)
            << " best_ask=" << formatOptionalPrice(instrument.best_ask) << '\n';
    }
}

std::uint64_t ShardedLobMarketDataEventProcessor::resolveInstrumentId(
    const MarketDataEvent& event
) const {
    if (event.instrument_id != 0) {
        return event.instrument_id;
    }

    if (event.order_id == 0) {
        return 0;
    }

    const auto it = router_orders_.find(event.order_id);
    if (it == router_orders_.end()) {
        return 0;
    }

    return it->second.instrument_id;
}

std::size_t ShardedLobMarketDataEventProcessor::workerIndex(
    std::uint64_t instrument_id
) const noexcept {
    return static_cast<std::size_t>(instrument_id % workers_.size());
}

void ShardedLobMarketDataEventProcessor::updateRouterAndDispatch(MarketDataEvent event) {
    switch (event.action) {
        case Action::Add:
        case Action::Modify:
            if (hasValidRestingState(event)) {
                const auto it = router_orders_.find(event.order_id);
                if (it != router_orders_.end() && it->second.instrument_id != event.instrument_id) {
                    enqueueFullCancel(it->second.instrument_id, event.order_id, event);
                }
                router_orders_[event.order_id] = RouterOrder{
                    .instrument_id = event.instrument_id,
                    .size = event.size,
                };
            }
            break;
        case Action::Cancel: {
            const auto it = router_orders_.find(event.order_id);
            if (it != router_orders_.end()) {
                if (event.size == 0 || event.size >= it->second.size) {
                    router_orders_.erase(it);
                } else {
                    it->second.size -= event.size;
                }
            }
            break;
        }
        case Action::Clear:
            eraseRouterOrdersForInstrument(event.instrument_id);
            break;
        case Action::Trade:
        case Action::Fill:
        case Action::None:
            break;
    }

    workers_[workerIndex(event.instrument_id)]->enqueue(std::move(event));
}

void ShardedLobMarketDataEventProcessor::enqueueFullCancel(
    std::uint64_t instrument_id,
    std::uint64_t order_id,
    const MarketDataEvent& source
) {
    MarketDataEvent cancel = source;
    cancel.instrument_id = instrument_id;
    cancel.order_id = order_id;
    cancel.action = Action::Cancel;
    cancel.size = 0;
    workers_[workerIndex(instrument_id)]->enqueue(std::move(cancel));
}

void ShardedLobMarketDataEventProcessor::eraseRouterOrdersForInstrument(
    std::uint64_t instrument_id
) {
    for (auto it = router_orders_.begin(); it != router_orders_.end();) {
        if (it->second.instrument_id == instrument_id) {
            it = router_orders_.erase(it);
        } else {
            ++it;
        }
    }
}

void ShardedLobMarketDataEventProcessor::maybePrintSnapshot(std::uint64_t timestamp) {
    if (config_.snapshot_interval_events == 0 || snapshot_count_ >= config_.max_snapshots) {
        return;
    }

    if (processed_count_ % config_.snapshot_interval_events != 0) {
        return;
    }

    snapshot_writer_.write(mergedSnapshot(
        processed_count_,
        timestamp,
        config_.snapshot_depth
    ));
    ++snapshot_count_;
}

void ShardedLobMarketDataEventProcessor::drainWorkers() {
    for (auto& worker : workers_) {
        worker->drain();
    }
}

BookManagerSnapshot ShardedLobMarketDataEventProcessor::mergedSnapshot(
    std::size_t event_count,
    std::uint64_t timestamp,
    std::size_t depth
) {
    drainWorkers();

    BookManagerSnapshot merged;
    merged.event_count = event_count;
    merged.timestamp = timestamp;
    merged.processed_events = processed_count_;
    merged.unresolved_events = totalUnresolvedEvents();

    for (const auto& worker : workers_) {
        auto worker_snapshot = worker->books().snapshot(event_count, timestamp, depth);
        for (auto& instrument : worker_snapshot.instruments) {
            merged.instruments.push_back(std::move(instrument));
        }
    }

    std::sort(merged.instruments.begin(), merged.instruments.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.instrument_id < rhs.instrument_id;
    });

    return merged;
}

std::size_t ShardedLobMarketDataEventProcessor::totalUnresolvedEvents() {
    drainWorkers();

    std::size_t total = router_unresolved_events_;
    for (const auto& worker : workers_) {
        total += worker->books().unresolvedEvents();
    }
    return total;
}

} // namespace md
