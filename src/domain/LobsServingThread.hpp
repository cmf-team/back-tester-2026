#pragma once

#include "domain/LimitOrderBookSingleInstrument.hpp"
#include "transport/MarketEventQueue.hpp"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace domain {
using Lob = LimitOrderBookSingleInstrument;
using LobSptr = Lob::Sptr;
using InstrumentIdToLob = std::unordered_map<InstrumentId, LobSptr>;











class LobsServingThread final {
public:
  using Sptr = std::shared_ptr<LobsServingThread>;

  struct CompareByCapacity {
    bool operator()(const Sptr &a, const Sptr &b) const {
      return a->capacity() > b->capacity();
    }
  };

  LobsServingThread();
  ~LobsServingThread();

  LobsServingThread(const LobsServingThread &) = delete;
  LobsServingThread &operator=(const LobsServingThread &) = delete;
  LobsServingThread(LobsServingThread &&) = delete;
  LobsServingThread &operator=(LobsServingThread &&) = delete;

  void start();
  void stop();




  void attachLob(InstrumentId instrument_id, const LobSptr &lob);

  void putEvent(const MarketDataEvent &event);
  std::size_t capacity() const;





  void pause();
  void resume();

private:
  transport::MarketEventsQueue market_events_queue_;

  std::thread thread_;
  mutable std::mutex lobs_m_;
  InstrumentIdToLob lobs_;


  mutable std::mutex pause_m_;
  std::condition_variable pause_cv_;
  bool pause_requested_{false};
  bool worker_paused_{false};
};
}
