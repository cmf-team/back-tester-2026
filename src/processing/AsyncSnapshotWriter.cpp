#include "processing/AsyncSnapshotWriter.hpp"

#include "domain/MarketDataEvent.hpp"

#include <ostream>
#include <string>
#include <utility>

namespace md {
namespace {

std::string formatOptionalPrice(std::optional<std::int64_t> price) {
    return price.has_value() ? formatPrice(*price) : "<none>";
}

} // namespace

AsyncSnapshotWriter::AsyncSnapshotWriter(std::ostream& out, SnapshotWriterMode mode)
    : out_(out), mode_(mode) {
    if (mode_ == SnapshotWriterMode::Async) {
        worker_ = std::thread([this] {
            workerLoop();
        });
    }
}

AsyncSnapshotWriter::~AsyncSnapshotWriter() {
    finish();
}

void AsyncSnapshotWriter::write(BookManagerSnapshot snapshot) {
    if (mode_ == SnapshotWriterMode::Sync) {
        ++submitted_count_;
        writeNow(snapshot);
        ++written_count_;
        return;
    }

    {
        std::lock_guard lock{mutex_};
        if (finish_requested_) {
            return;
        }
        queue_.push_back(std::move(snapshot));
        ++submitted_count_;
    }
    cv_.notify_one();
}

void AsyncSnapshotWriter::finish() {
    if (mode_ == SnapshotWriterMode::Sync) {
        return;
    }

    {
        std::lock_guard lock{mutex_};
        finish_requested_ = true;
    }
    cv_.notify_one();

    if (worker_.joinable()) {
        worker_.join();
    }
}

std::size_t AsyncSnapshotWriter::submittedCount() const noexcept {
    return submitted_count_.load();
}

std::size_t AsyncSnapshotWriter::writtenCount() const noexcept {
    return written_count_.load();
}

void AsyncSnapshotWriter::workerLoop() {
    while (true) {
        BookManagerSnapshot snapshot;
        {
            std::unique_lock lock{mutex_};
            cv_.wait(lock, [this] {
                return finish_requested_ || !queue_.empty();
            });

            if (queue_.empty()) {
                if (finish_requested_) {
                    return;
                }
                continue;
            }

            snapshot = std::move(queue_.front());
            queue_.pop_front();
        }

        writeNow(snapshot);
        ++written_count_;
    }
}

void AsyncSnapshotWriter::writeNow(const BookManagerSnapshot& snapshot) {
    printBookManagerSnapshot(snapshot, out_);
}

void printBookManagerSnapshot(const BookManagerSnapshot& snapshot, std::ostream& out) {
    out << "LOB Snapshot\n"
        << "event_count=" << snapshot.event_count << '\n'
        << "timestamp=" << snapshot.timestamp << '\n'
        << "BookManager snapshot"
        << " instruments=" << snapshot.instruments.size()
        << " processed_events=" << snapshot.processed_events
        << " unresolved_events=" << snapshot.unresolved_events << '\n';

    for (const auto& instrument : snapshot.instruments) {
        out << "instrument_id=" << instrument.instrument_id
            << " resting_orders=" << instrument.resting_orders
            << " best_bid=" << formatOptionalPrice(instrument.best_bid)
            << " best_ask=" << formatOptionalPrice(instrument.best_ask) << '\n';

        out << "  bids:\n";
        for (const auto& level : instrument.bids) {
            out << "    " << formatPrice(level.price) << " x " << level.size << '\n';
        }

        out << "  asks:\n";
        for (const auto& level : instrument.asks) {
            out << "    " << formatPrice(level.price) << " x " << level.size << '\n';
        }
    }
}

} // namespace md
