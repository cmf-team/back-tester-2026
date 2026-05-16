#pragma once

#include "book/BookSnapshot.hpp"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iosfwd>
#include <mutex>
#include <thread>

namespace md {

enum class SnapshotWriterMode {
    Sync,
    Async
};

class AsyncSnapshotWriter {
public:
    explicit AsyncSnapshotWriter(
        std::ostream& out,
        SnapshotWriterMode mode = SnapshotWriterMode::Sync
    );
    ~AsyncSnapshotWriter();

    AsyncSnapshotWriter(const AsyncSnapshotWriter&) = delete;
    AsyncSnapshotWriter& operator=(const AsyncSnapshotWriter&) = delete;

    void write(BookManagerSnapshot snapshot);
    void finish();

    [[nodiscard]] std::size_t submittedCount() const noexcept;
    [[nodiscard]] std::size_t writtenCount() const noexcept;

private:
    void workerLoop();
    void writeNow(const BookManagerSnapshot& snapshot);

    std::ostream& out_;
    SnapshotWriterMode mode_{SnapshotWriterMode::Sync};
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<BookManagerSnapshot> queue_;
    std::thread worker_;
    bool finish_requested_{false};
    std::atomic<std::size_t> submitted_count_{};
    std::atomic<std::size_t> written_count_{};
};

void printBookManagerSnapshot(const BookManagerSnapshot& snapshot, std::ostream& out);

} // namespace md
