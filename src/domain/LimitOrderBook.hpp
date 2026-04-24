#pragma once

#include "domain/LobsServingThread.hpp"

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace domain {
using InstrumentIdToBidsBook = std::unordered_map<InstrumentId, BidsBookMap>;
using InstrumentIdToAsksBook = std::unordered_map<InstrumentId, AsksBookMap>;
using InstrumentIdToBestQuote = std::unordered_map<InstrumentId, BestQuote>;
using InstrumentIdToExecStats = std::unordered_map<InstrumentId, ExecStats>;





class LobConsistentView final {
public:
  LobConsistentView() = default;
  LobConsistentView(std::vector<LobsServingThread::Sptr> threads,
                    std::unique_lock<std::mutex> freeze_lock);
  ~LobConsistentView();

  LobConsistentView(const LobConsistentView &) = delete;
  LobConsistentView &operator=(const LobConsistentView &) = delete;
  LobConsistentView(LobConsistentView &&other) noexcept;
  LobConsistentView &operator=(LobConsistentView &&other) noexcept;

private:
  void resumeAll() noexcept;

  std::vector<LobsServingThread::Sptr> paused_threads_;
  std::unique_lock<std::mutex> freeze_lock_;
};

template <typename QueueT> class LimitOrderBook final {
public:
  explicit LimitOrderBook(std::size_t max_work_threads_nums = 1);

  void onEvent(const MarketDataEvent &event);
  void onEndEvent();





  LobConsistentView freeze();

  BidsBookMap getBids(InstrumentId instrument_id) const;
  AsksBookMap getAsks(InstrumentId instrument_id) const;
  BestQuote getBestBid(InstrumentId instrument_id) const;
  BestQuote getBestAsk(InstrumentId instrument_id) const;
  Quantity getVolumeAtPrice(InstrumentId instrument_id, char side,
                            Price price) const;

  InstrumentIdToBidsBook getAllBids() const;
  InstrumentIdToAsksBook getAllAsks() const;
  InstrumentIdToBestQuote getAllBestBids() const;
  InstrumentIdToBestQuote getAllBestAsks() const;
  InstrumentIdToExecStats getAllTradeStats() const;
  InstrumentIdToExecStats getAllFillStats() const;

private:
  using ThreadSptr = LobsServingThread::Sptr;
  using WorkThreadsHeap =
      std::priority_queue<ThreadSptr, std::vector<ThreadSptr>,
                          LobsServingThread::CompareByCapacity>;

  std::optional<InstrumentId>
  resolveInstrumentId(const MarketDataEvent &event) const;
  void updateOrderRouting(const MarketDataEvent &event,
                          InstrumentId instrument_id);
  void clearInstrumentOrders(InstrumentId instrument_id);

  void registerInstrument(InstrumentId instrument_id);
  ThreadSptr acquireLeastLoadedThread();
  LobSptr findLob(InstrumentId instrument_id) const;

  std::size_t max_work_threads_nums_;

  InstrumentIdToLob instrument_to_lob_;
  std::unordered_map<InstrumentId, ThreadSptr> instrument_id_to_thread_;
  WorkThreadsHeap work_threads_;

  std::unordered_map<OrderId, InstrumentId> order_to_instrument_;
  std::unordered_map<InstrumentId, std::unordered_set<OrderId>>
      instrument_to_orders_;




  std::mutex freeze_m_;
};
}
