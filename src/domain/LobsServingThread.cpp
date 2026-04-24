#include "domain/LobsServingThread.hpp"

namespace domain {
namespace {



const events::MarketDataEvent kPauseMarker{
    .ts_recv = {},
    .hd = {},
    .action = {},
    .side = {},
    .price = {},
    .size = {},
    .channel_id = {},
    .order_id = {},
    .flags = {},
    .ts_in_delta = {},
    .sequence = {},
    .symbol = "__PAUSE_MARKER__",
};

bool isPauseMarker(const events::MarketDataEvent &event) {
  return event.symbol == kPauseMarker.symbol;
}

bool isEof(const events::MarketDataEvent &event) {
  return event.symbol == events::EOF_EVENT.symbol;
}

}

LobsServingThread::LobsServingThread() = default;

LobsServingThread::~LobsServingThread() { stop(); }

void LobsServingThread::start() {
  thread_ = std::thread([this]() {
    for (;;) {
      auto event = market_events_queue_.popLatestEvent();

      if (isEof(event)) {
        break;
      }

      if (isPauseMarker(event)) {
        std::unique_lock<std::mutex> lock(pause_m_);
        worker_paused_ = true;
        pause_cv_.notify_all();
        pause_cv_.wait(lock, [this] { return !pause_requested_; });
        worker_paused_ = false;
        pause_cv_.notify_all();
        continue;
      }

      LobSptr lob;
      {
        const std::lock_guard<std::mutex> lock(lobs_m_);
        const auto it = lobs_.find(event.hd.instrument_id);
        if (it != lobs_.end()) {
          lob = it->second;
        }
      }
      if (lob) {
        lob->onEvent(event);
      }
    }
  });
}

void LobsServingThread::stop() {


  {
    const std::lock_guard<std::mutex> lock(pause_m_);
    pause_requested_ = false;
    pause_cv_.notify_all();
  }
  market_events_queue_.put(events::EOF_EVENT);
  if (thread_.joinable()) {
    thread_.join();
  }
}

void LobsServingThread::attachLob(const InstrumentId instrument_id,
                                  const LobSptr &lob) {
  const std::lock_guard<std::mutex> lock(lobs_m_);
  lobs_.emplace(instrument_id, lob);
}

void LobsServingThread::putEvent(const MarketDataEvent &event) {
  market_events_queue_.put(event);
}

std::size_t LobsServingThread::capacity() const {
  const std::lock_guard<std::mutex> lock(lobs_m_);
  return lobs_.size();
}

void LobsServingThread::pause() {
  std::unique_lock<std::mutex> lock(pause_m_);
  pause_requested_ = true;


  market_events_queue_.put(kPauseMarker);
  pause_cv_.wait(lock, [this] { return worker_paused_; });
}

void LobsServingThread::resume() {
  std::unique_lock<std::mutex> lock(pause_m_);
  pause_requested_ = false;
  pause_cv_.notify_all();


  pause_cv_.wait(lock, [this] { return !worker_paused_; });
}

}
