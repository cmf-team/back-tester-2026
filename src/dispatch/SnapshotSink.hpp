#pragma once

#include "common/BasicTypes.hpp"
#include "common/SpscQueue.hpp"
#include "dispatch/EventSink.hpp"
#include "dispatch/LobRegistry.hpp"
#include "lob/LimitOrderBook.hpp"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <thread>

namespace cmf {

struct SnapshotFrame {
    NanoTime              ts            = 0;
    uint32_t              instrument_id = 0;
    uint8_t               n_bids        = 0;
    uint8_t               n_asks        = 0;
    LimitOrderBook::Level bids[10]      = {};
    LimitOrderBook::Level asks[10]      = {};
};

class SnapshotSink : public EventSink {
public:
    SnapshotSink(const LobRegistry& reg, std::filesystem::path out_path, std::size_t every) noexcept
        : reg_(reg), out_path_(std::move(out_path)), every_(every) {}

    ~SnapshotSink() override { stop(); }

    void start() {
        if (out_path_.empty() || every_ == 0) return;
        running_.store(true, std::memory_order_release);
        writer_ = std::thread(&SnapshotSink::run, this);
    }

    void stop() {
        if (!running_.load(std::memory_order_acquire) && !writer_.joinable()) return;
        SnapshotFrame done{};
        done.ts = MarketDataEvent::SENTINEL;
        queue_.push(done);
        if (writer_.joinable()) writer_.join();
        running_.store(false, std::memory_order_release);
    }

    void on_event(const MarketDataEvent& e, const LimitOrderBook& book) override {
        if (every_ == 0) return;
        if (++since_ < every_) return;
        since_ = 0;
        SnapshotFrame f;
        f.ts            = e.ts_recv;
        f.instrument_id = book.instrument_id();
        f.n_bids = static_cast<uint8_t>(book.snapshot_bids(f.bids));
        f.n_asks = static_cast<uint8_t>(book.snapshot_asks(f.asks));
        queue_.push(f);
    }

    std::size_t frames_published() const noexcept { return published_; }

private:
    void run() {
        std::FILE* fh = std::fopen(out_path_.c_str(), "w");
        if (!fh) return;
        std::fprintf(fh, "ts,instrument_id,bid_px,bid_qty,ask_px,ask_qty\n");
        SnapshotFrame f;
        while (true) {
            queue_.pop(f);
            if (f.ts == MarketDataEvent::SENTINEL) break;
            const double bp = f.n_bids ? f.bids[0].price : 0.0;
            const auto   bq = f.n_bids ? f.bids[0].qty   : 0u;
            const double ap = f.n_asks ? f.asks[0].price : 0.0;
            const auto   aq = f.n_asks ? f.asks[0].qty   : 0u;
            std::fprintf(fh, "%ld,%u,%.9f,%lu,%.9f,%lu\n",
                         f.ts, f.instrument_id, bp, bq, ap, aq);
            ++published_;
        }
        std::fclose(fh);
    }

    const LobRegistry&                reg_;
    std::filesystem::path             out_path_;
    std::size_t                       every_;
    std::size_t                       since_     = 0;
    std::size_t                       published_ = 0;
    SpscQueue<SnapshotFrame, 4096>    queue_;
    std::thread                       writer_;
    std::atomic<bool>                 running_{false};
};

} // namespace cmf
