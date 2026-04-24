#include "domain/LimitOrderBook.hpp"

#include "transport/FlatSyncedQueue.hpp"
#include "transport/HierarchicalSyncedQueue.hpp"

#include <unordered_set>
#include <utility>

namespace domain {

LobConsistentView::LobConsistentView(
    std::vector<LobsServingThread::Sptr> threads,
    std::unique_lock<std::mutex> freeze_lock)
    : paused_threads_(std::move(threads)),
      freeze_lock_(std::move(freeze_lock)) {}

LobConsistentView::~LobConsistentView() { resumeAll(); }

LobConsistentView::LobConsistentView(LobConsistentView &&other) noexcept
    : paused_threads_(std::move(other.paused_threads_)),
      freeze_lock_(std::move(other.freeze_lock_)) {
  other.paused_threads_.clear();
}

LobConsistentView &
LobConsistentView::operator=(LobConsistentView &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  resumeAll();
  paused_threads_ = std::move(other.paused_threads_);
  freeze_lock_ = std::move(other.freeze_lock_);
  other.paused_threads_.clear();
  return *this;
}

void LobConsistentView::resumeAll() noexcept {
  for (const auto &thread : paused_threads_) {
    thread->resume();
  }
  paused_threads_.clear();
}

template <typename QueueT>
LimitOrderBook<QueueT>::LimitOrderBook(const std::size_t max_work_threads_nums)
    : max_work_threads_nums_(max_work_threads_nums) {}

template <typename QueueT>
void LimitOrderBook<QueueT>::onEvent(const MarketDataEvent &event) {
  const auto resolved_instrument_id = resolveInstrumentId(event);
  if (!resolved_instrument_id.has_value()) {
    return;
  }

  const auto instrument_id = *resolved_instrument_id;
  updateOrderRouting(event, instrument_id);

  if (event.action == 'R') {
    clearInstrumentOrders(instrument_id);
  }

  registerInstrument(instrument_id);

  auto routed_event = event;
  routed_event.hd.instrument_id = instrument_id;
  instrument_id_to_thread_[instrument_id]->putEvent(routed_event);
}

template <typename QueueT> void LimitOrderBook<QueueT>::onEndEvent() {
  std::unordered_set<const LobsServingThread *> stopped;
  for (const auto &[_, thread] : instrument_id_to_thread_) {
    if (stopped.insert(thread.get()).second) {
      thread->stop();
    }
  }
}

template <typename QueueT>
LobConsistentView LimitOrderBook<QueueT>::freeze() {


  std::unique_lock<std::mutex> freeze_lock(freeze_m_);

  std::vector<LobsServingThread::Sptr> unique_threads;
  std::unordered_set<const LobsServingThread *> seen;
  unique_threads.reserve(instrument_id_to_thread_.size());
  for (const auto &[_, thread] : instrument_id_to_thread_) {
    if (seen.insert(thread.get()).second) {
      unique_threads.push_back(thread);
    }
  }

  for (const auto &thread : unique_threads) {
    thread->pause();
  }
  return LobConsistentView{std::move(unique_threads), std::move(freeze_lock)};
}

template <typename QueueT>
void LimitOrderBook<QueueT>::registerInstrument(const InstrumentId instrument_id) {
  if (instrument_to_lob_.count(instrument_id) != 0) {
    return;
  }

  auto lob = std::make_shared<Lob>();
  instrument_to_lob_.emplace(instrument_id, lob);

  auto thread = acquireLeastLoadedThread();
  thread->attachLob(instrument_id, lob);
  work_threads_.push(thread);
  instrument_id_to_thread_[instrument_id] = thread;
}

template <typename QueueT>
typename LimitOrderBook<QueueT>::ThreadSptr
LimitOrderBook<QueueT>::acquireLeastLoadedThread() {
  if (work_threads_.size() < max_work_threads_nums_) {
    auto thread = std::make_shared<LobsServingThread>();
    thread->start();
    return thread;
  }
  auto thread = work_threads_.top();
  work_threads_.pop();
  return thread;
}

template <typename QueueT>
LobSptr
LimitOrderBook<QueueT>::findLob(const InstrumentId instrument_id) const {
  const auto it = instrument_to_lob_.find(instrument_id);
  return it == instrument_to_lob_.end() ? LobSptr{} : it->second;
}

template <typename QueueT>
BidsBookMap
LimitOrderBook<QueueT>::getBids(const InstrumentId instrument_id) const {
  const auto lob = findLob(instrument_id);
  return lob ? lob->getBids() : BidsBookMap{};
}

template <typename QueueT>
AsksBookMap
LimitOrderBook<QueueT>::getAsks(const InstrumentId instrument_id) const {
  const auto lob = findLob(instrument_id);
  return lob ? lob->getAsks() : AsksBookMap{};
}

template <typename QueueT>
BestQuote
LimitOrderBook<QueueT>::getBestBid(const InstrumentId instrument_id) const {
  const auto lob = findLob(instrument_id);
  return lob ? lob->getBestBid() : BestQuote{};
}

template <typename QueueT>
BestQuote
LimitOrderBook<QueueT>::getBestAsk(const InstrumentId instrument_id) const {
  const auto lob = findLob(instrument_id);
  return lob ? lob->getBestAsk() : BestQuote{};
}

template <typename QueueT>
Quantity LimitOrderBook<QueueT>::getVolumeAtPrice(
    const InstrumentId instrument_id, const char side, const Price price) const {
  const auto lob = findLob(instrument_id);
  return lob ? lob->getVolumeAtPrice(side, price) : Quantity{0};
}

template <typename QueueT>
InstrumentIdToBidsBook LimitOrderBook<QueueT>::getAllBids() const {
  InstrumentIdToBidsBook result;
  result.reserve(instrument_to_lob_.size());
  for (const auto &[instrument_id, lob] : instrument_to_lob_) {
    result.emplace(instrument_id, lob->getBids());
  }
  return result;
}

template <typename QueueT>
InstrumentIdToAsksBook LimitOrderBook<QueueT>::getAllAsks() const {
  InstrumentIdToAsksBook result;
  result.reserve(instrument_to_lob_.size());
  for (const auto &[instrument_id, lob] : instrument_to_lob_) {
    result.emplace(instrument_id, lob->getAsks());
  }
  return result;
}

template <typename QueueT>
InstrumentIdToBestQuote LimitOrderBook<QueueT>::getAllBestBids() const {
  InstrumentIdToBestQuote result;
  result.reserve(instrument_to_lob_.size());
  for (const auto &[instrument_id, lob] : instrument_to_lob_) {
    result.emplace(instrument_id, lob->getBestBid());
  }
  return result;
}

template <typename QueueT>
InstrumentIdToBestQuote LimitOrderBook<QueueT>::getAllBestAsks() const {
  InstrumentIdToBestQuote result;
  result.reserve(instrument_to_lob_.size());
  for (const auto &[instrument_id, lob] : instrument_to_lob_) {
    result.emplace(instrument_id, lob->getBestAsk());
  }
  return result;
}

template <typename QueueT>
InstrumentIdToExecStats LimitOrderBook<QueueT>::getAllTradeStats() const {
  InstrumentIdToExecStats result;
  result.reserve(instrument_to_lob_.size());
  for (const auto &[instrument_id, lob] : instrument_to_lob_) {
    result.emplace(instrument_id, lob->getTradeStats());
  }
  return result;
}

template <typename QueueT>
InstrumentIdToExecStats LimitOrderBook<QueueT>::getAllFillStats() const {
  InstrumentIdToExecStats result;
  result.reserve(instrument_to_lob_.size());
  for (const auto &[instrument_id, lob] : instrument_to_lob_) {
    result.emplace(instrument_id, lob->getFillStats());
  }
  return result;
}

template <typename QueueT>
std::optional<InstrumentId>
LimitOrderBook<QueueT>::resolveInstrumentId(const MarketDataEvent &event) const {
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

template <typename QueueT>
void LimitOrderBook<QueueT>::updateOrderRouting(const MarketDataEvent &event,
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

template <typename QueueT>
void LimitOrderBook<QueueT>::clearInstrumentOrders(const InstrumentId instrument_id) {
  const auto it = instrument_to_orders_.find(instrument_id);
  if (it == instrument_to_orders_.end()) {
    return;
  }
  for (const auto order_id : it->second) {
    order_to_instrument_.erase(order_id);
  }
  instrument_to_orders_.erase(it);
}

template class LimitOrderBook<transport::FlatSyncedQueue>;
template class LimitOrderBook<transport::HierarchicalSyncedQueue>;
}
