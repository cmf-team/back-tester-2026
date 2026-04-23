#pragma once

#include "common/BasicTypes.hpp"
#include "common/Events.hpp"
#include "transport/MarketEventQueue.hpp"
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace domain {
using namespace cmf;
using MarketDataEvent = events::MarketDataEvent;

using InstrumentId = decltype(std::declval<MarketDataEvent>().hd.instrument_id);

class LimitOrderBookSingleInstrument final {
public:
  struct OrderState {
    char side{};
    Price price{};
    Quantity size{};
  };

  void onAdd(const MarketDataEvent &event) {
    if (!isBookSide(event.side) || event.size <= 0) {
      return;
    }

    if (const auto it = orders_.find(event.order_id); it != orders_.end()) {
      removeOrderContribution(it->second);
      orders_.erase(it);
    }

    const OrderState state{event.side, static_cast<Price>(event.price),
                           static_cast<Quantity>(event.size)};
    orders_[event.order_id] = state;
    addOrderContribution(state);
  }

  void onModify(const MarketDataEvent &event) {
    const auto it = orders_.find(event.order_id);
    if (it == orders_.end()) {
      // Graceful fallback for out-of-order/partial datasets.
      onAdd(event);
      return;
    }

    removeOrderContribution(it->second);

    const char new_side = isBookSide(event.side) ? event.side : it->second.side;
    const Quantity new_size = static_cast<Quantity>(event.size);
    if (new_size <= 0 || !isBookSide(new_side)) {
      orders_.erase(it);
      return;
    }

    it->second.side = new_side;
    it->second.price = event.price;
    it->second.size = new_size;
    addOrderContribution(it->second);
  }

  void onCancel(const MarketDataEvent &event) {
    const auto it = orders_.find(event.order_id);
    if (it == orders_.end()) {
      return;
    }

    const Quantity cancel_size = static_cast<Quantity>(event.size);
    if (cancel_size <= 0) {
      return;
    }

    const Quantity removed = std::min(it->second.size, cancel_size);
    if (removed <= 0) {
      return;
    }

    adjustLevel(it->second.side, it->second.price, -removed);
    it->second.size -= removed;
    if (it->second.size <= 0) {
      orders_.erase(it);
    }
  }

  void onClear() {
    bids_.clear();
    asks_.clear();
    orders_.clear();
  }

  void onEvent(const MarketDataEvent &event) {
    std::unique_lock<std::mutex> lock(m_);

    switch (event.action) {
    case 'A':
      onAdd(event);
      break;
    case 'M':
      onModify(event);
      break;
    case 'C':
      onCancel(event);
      break;
    case 'R':
      onClear();
      break;
    default:
      break;
    }
  }

  std::map<Price, Quantity, std::greater<Price>> getBids() const {
    std::lock_guard<std::mutex> lock(m_);
    return bids_;
  }

  std::map<Price, Quantity, std::less<Price>> getAsks() const {
    std::lock_guard<std::mutex> lock(m_);
    return asks_;
  }

private:
  static bool isBookSide(char side) { return side == 'B' || side == 'A'; }

  void addOrderContribution(const OrderState &state) {
    adjustLevel(state.side, state.price, state.size);
  }

  void removeOrderContribution(const OrderState &state) {
    adjustLevel(state.side, state.price, -state.size);
  }

  void adjustLevel(const char side, const Price price, const Quantity delta) {
    if (delta == 0) {
      return;
    }

    auto adjust = [&](auto &book) {
      auto &level = book[price];
      level += delta;
      if (level <= 0) {
        book.erase(price);
      }
    };

    if (side == 'B') {
      adjust(bids_);
    } else if (side == 'A') {
      adjust(asks_);
    }
  }

  mutable std::mutex m_; // guard bids, asks, orders
  std::map<Price, Quantity, std::greater<Price>> bids_;
  std::map<Price, Quantity, std::less<Price>> asks_;
  std::unordered_map<OrderId, OrderState> orders_;
};

class LobsServingThread final {
public:
  using Sptr = std::shared_ptr<LobsServingThread>;

  struct CompareByCapacity {
    bool operator()(const Sptr &a, const Sptr &b) const {
      return a->capacity() > b->capacity();
    }
  };

  explicit LobsServingThread(
      const InstrumentId instrument_id,
      const std::shared_ptr<LimitOrderBookSingleInstrument> &lob) {
    lobs_.emplace(instrument_id, lob);
  }

  ~LobsServingThread() { stop(); }

  LobsServingThread(const LobsServingThread &) = delete;
  LobsServingThread &operator=(const LobsServingThread &) = delete;
  LobsServingThread(LobsServingThread &&) = delete;
  LobsServingThread &operator=(LobsServingThread &&) = delete;

  void start() {
    thread_ = std::thread([this]() {
      while (!stop_requested_) {
        auto event = market_events_queue_.popLatestEvent();
        if (event.symbol != domain::events::EOF_EVENT.symbol) {
          std::shared_ptr<LimitOrderBookSingleInstrument> lob;
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
        } else {
          break;
        }
      }
    });
  }

  void stop() {
    stop_requested_ = true;
    market_events_queue_.put(domain::events::EOF_EVENT);
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void addLob(const InstrumentId instrument_id,
              const std::shared_ptr<LimitOrderBookSingleInstrument> &lob) {
    const std::lock_guard<std::mutex> lock(lobs_m_);
    lobs_.emplace(instrument_id, lob);
  }

  size_t capacity() const { return lobs_.size(); }

  void putEvent(const MarketDataEvent &event) {
    market_events_queue_.put(event);
  }

  // for logging only
  std::unordered_map<InstrumentId,
                     std::map<Price, Quantity, std::greater<Price>>>
  getBidsForInstruments() const {
    std::unordered_map<InstrumentId,
                       std::map<Price, Quantity, std::greater<Price>>>
        bids;
    for (const auto &[instrument_id, lob] : lobs_) {
      bids[instrument_id] = lob->getBids();
    }
    return bids;
  }

  std::unordered_map<InstrumentId, std::map<Price, Quantity, std::less<Price>>>
  getAsksForInstruments() const {
    std::unordered_map<InstrumentId,
                       std::map<Price, Quantity, std::less<Price>>>
        asks;
    for (const auto &[instrument_id, lob] : lobs_) {
      asks[instrument_id] = lob->getAsks();
    }
    return asks;
  }
  // end of logging only

private:
  std::atomic<bool> stop_requested_{false};
  transport::MarketEventsQueue market_events_queue_;

  std::thread thread_;
  std::mutex lobs_m_;
  std::unordered_map<InstrumentId,
                     std::shared_ptr<LimitOrderBookSingleInstrument>>
      lobs_;
};

template <typename QueueT> class LimitOrderBook final {
public:
  explicit LimitOrderBook(const size_t max_work_threads_nums = 1)
      : max_work_threads_nums_(max_work_threads_nums) {}

  void onEvent(const MarketDataEvent &event) {
    const auto resolved_instrument_id = resolveInstrumentId(event);
    if (!resolved_instrument_id.has_value()) {
      return;
    }

    const auto instrument_id = *resolved_instrument_id;
    updateOrderRouting(event, instrument_id);

    if (event.action == 'R') {
      clearInstrumentOrders(instrument_id);
    }

    if (existing_instruments_.count(instrument_id) == 0) {
      if (work_threads_.size() == max_work_threads_nums_) {
        addNewLob(instrument_id);
      } else {
        startNewThread(instrument_id);
      }
    }

    auto routed_event = event;
    routed_event.hd.instrument_id = instrument_id;
    instrument_id_to_thread_[instrument_id]->putEvent(routed_event);
  }

  void onEndEvent() {
    for (const auto &thread : instrument_id_to_thread_) {
      thread.second->stop();
    }
  }

  void startNewThread(const InstrumentId instrument_id) {
    existing_instruments_.insert(instrument_id);

    instrument_id_to_thread_.emplace(
        instrument_id,
        std::make_shared<LobsServingThread>(
            instrument_id, std::make_shared<LimitOrderBookSingleInstrument>()));
    instrument_id_to_thread_[instrument_id]->start();

    work_threads_.push(instrument_id_to_thread_[instrument_id]);
  }

  void addNewLob(const InstrumentId instrument_id) {
    existing_instruments_.insert(instrument_id);
    const auto thread = work_threads_.top();
    work_threads_.pop();
    thread->addLob(instrument_id,
                   std::make_shared<LimitOrderBookSingleInstrument>());
    work_threads_.push(thread);
    instrument_id_to_thread_[instrument_id] = thread;
  }

  // for logging only
  std::unordered_map<InstrumentId,
                     std::map<Price, Quantity, std::greater<Price>>>
  getAllBids() {
    std::unordered_map<InstrumentId,
                       std::map<Price, Quantity, std::greater<Price>>>
        bids;
    for (const auto &[instrument_id, thread] : instrument_id_to_thread_) {
      const auto thread_bids = thread->getBidsForInstruments();
      const auto it = thread_bids.find(instrument_id);
      if (it != thread_bids.end()) {
        bids.emplace(instrument_id, it->second);
      }
    }
    return bids;
  }

  std::unordered_map<InstrumentId, std::map<Price, Quantity, std::less<Price>>>
  getAllAsks() {
    std::unordered_map<InstrumentId,
                       std::map<Price, Quantity, std::less<Price>>>
        asks;
    for (const auto &[instrument_id, thread] : instrument_id_to_thread_) {
      const auto thread_asks = thread->getAsksForInstruments();
      const auto it = thread_asks.find(instrument_id);
      if (it != thread_asks.end()) {
        asks.emplace(instrument_id, it->second);
      }
    }
    return asks;
  }
  // end of logging only

private:
  std::optional<InstrumentId> resolveInstrumentId(
      const MarketDataEvent &event) const {
    if (event.hd.instrument_id != 0) {
      return event.hd.instrument_id;
    }
    if (event.order_id == 0) {
      return std::nullopt;
    }
    const auto it = order_to_instrument_.find(event.order_id);
    if (it == order_to_instrument_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void updateOrderRouting(const MarketDataEvent &event,
                          const InstrumentId instrument_id) {
    if (event.order_id == 0) {
      return;
    }

    const auto it = order_to_instrument_.find(event.order_id);
    if (it != order_to_instrument_.end() && it->second != instrument_id) {
      instrument_to_orders_[it->second].erase(event.order_id);
    }
    order_to_instrument_[event.order_id] = instrument_id;
    instrument_to_orders_[instrument_id].insert(event.order_id);
  }

  void clearInstrumentOrders(const InstrumentId instrument_id) {
    const auto it = instrument_to_orders_.find(instrument_id);
    if (it == instrument_to_orders_.end()) {
      return;
    }
    for (const auto order_id : it->second) {
      order_to_instrument_.erase(order_id);
    }
    instrument_to_orders_.erase(it);
  }

  size_t max_work_threads_nums_;

  std::unordered_map<InstrumentId, LobsServingThread::Sptr>
      instrument_id_to_thread_;

  std::priority_queue<LobsServingThread::Sptr,
                      std::vector<LobsServingThread::Sptr>,
                      LobsServingThread::CompareByCapacity>
      work_threads_;

  std::unordered_set<InstrumentId> existing_instruments_;
  std::unordered_map<OrderId, InstrumentId> order_to_instrument_;
  std::unordered_map<InstrumentId, std::unordered_set<OrderId>>
      instrument_to_orders_;
};
} // namespace domain